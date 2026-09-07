/*
 * mpqpack - minimal no-sudo MPQ packer for WoW 3.3.5a client patches.
 *
 * Built against a locally-compiled StormLib (see build-mpqpack.sh). This
 * tool exists so era-wow can ship a custom Spell.dbc (or any other client
 * file) inside an MPQ archive without needing a system package / sudo.
 *
 * Usage:
 *   mpqpack <archive.mpq> <internal\path> <source-file>
 *       Opens <archive.mpq> if it already exists (write-capable) so a second
 *       file can be added to the same patch archive across separate
 *       invocations, or creates it fresh if it doesn't exist yet. Either way,
 *       <source-file> is (re)added under <internal\path>, replacing any
 *       existing entry at that path. WoW internal MPQ paths use BACKSLASHES
 *       (e.g. DBFilesClient\Spell.dbc) - pass the path exactly as the caller
 *       wants it stored; this tool does not rewrite separators.
 *
 *   mpqpack verify <archive.mpq> <internal\path>
 *       Opens the archive read-only and checks the internal path is really
 *       present (SFileHasFile). Prints OK / MISSING and exits 0/1.
 *
 * Exit status: 0 on success, nonzero on any failure (message on stderr).
 */
#include <stdio.h>
#include <string.h>
#include <StormLib.h>

/* StormLib on Linux reports errors in errno-space (see StormPort.h), plus a
 * handful of StormLib-only codes (1000-1007) that aren't real errno values -
 * strerror() alone would print "Unknown error NNN" for those, so translate
 * the ones we're likely to hit and fall back to strerror() for the rest. */
static const char *mpq_strerror(DWORD err)
{
    switch (err) {
        case ERROR_BAD_FORMAT:             return "not a valid MPQ archive (bad format)";
        case ERROR_NO_MORE_FILES:          return "no more files";
        case ERROR_HANDLE_EOF:             return "unexpected end of file";
        case ERROR_CAN_NOT_COMPLETE:       return "operation could not complete";
        case ERROR_FILE_CORRUPT:           return "archive is corrupt";
        case ERROR_BUFFER_OVERFLOW:        return "buffer overflow";
        case ERROR_INVALID_DATA:           return "invalid data";
        case ERROR_NO_UNICODE_TRANSLATION: return "no unicode translation";
        default:                          return strerror((int)err);
    }
}

int main(int argc, char *argv[])
{
    /* Subcommand: verify an existing archive contains a given internal path. */
    if (argc == 4 && strcmp(argv[1], "verify") == 0) {
        const char *archivePath = argv[2];
        const char *internalPath = argv[3];
        HANDLE hMpq = NULL;

        if (!SFileOpenArchive(archivePath, 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
            fprintf(stderr, "mpqpack: SFileOpenArchive failed: %s\n", mpq_strerror(SErrGetLastError()));
            return 1;
        }

        int present = SFileHasFile(hMpq, internalPath);
        SFileCloseArchive(hMpq);

        if (!present) {
            fprintf(stderr, "MISSING: %s not found in %s\n", internalPath, archivePath);
            return 1;
        }
        printf("OK: %s found in %s\n", internalPath, archivePath);
        return 0;
    }

    /* Default mode: pack a single file into an existing-or-new archive. */
    if (argc != 4) {
        fprintf(stderr, "usage: %s <archive.mpq> <internal\\path> <source-file>\n", argv[0]);
        fprintf(stderr, "       %s verify <archive.mpq> <internal\\path>\n", argv[0]);
        return 1;
    }

    const char *archivePath = argv[1];
    const char *internalPath = argv[2];
    const char *sourcePath = argv[3];
    HANDLE hMpq = NULL;
    int createdNew = 0;

    /* Open-or-create: try opening the archive write-capable first (dwFlags=0
     * is NOT MPQ_OPEN_READ_ONLY) so re-running mpqpack against the same
     * archive adds/updates a file instead of hard-failing on EEXIST, and so
     * a later call can add a second DBC into the same patch. Only fall back
     * to SFileCreateArchive when the archive genuinely doesn't exist yet -
     * any other open failure (corrupt archive, permission denied, ...) is
     * reported and NOT silently papered over by creating on top of it. */
    if (!SFileOpenArchive(archivePath, 0, 0, &hMpq)) {
        DWORD openErr = SErrGetLastError();
        if (openErr != ERROR_FILE_NOT_FOUND) {
            fprintf(stderr, "mpqpack: SFileOpenArchive failed: %s\n", mpq_strerror(openErr));
            return 1;
        }

        /* Create a fresh v1 MPQ (up to 4GB, fine for a handful of DBC/patch
         * files) with a small hash table - HASH_TABLE_SIZE_MIN is plenty for
         * a few dozen files and keeps the archive small. */
        if (!SFileCreateArchive(archivePath, MPQ_CREATE_ARCHIVE_V1 | MPQ_CREATE_LISTFILE, HASH_TABLE_SIZE_MIN, &hMpq)) {
            fprintf(stderr, "mpqpack: SFileCreateArchive failed: %s\n", mpq_strerror(SErrGetLastError()));
            return 1;
        }
        createdNew = 1;
    }

    /* Add the source file under the given internal (backslash) path,
     * ZLIB-compressed, replacing any existing entry at that path. */
    if (!SFileAddFileEx(hMpq, sourcePath, internalPath,
                         MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING,
                         MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_ZLIB)) {
        DWORD addErr = SErrGetLastError();
        SFileCloseArchive(hMpq);
        /* If we just created this archive and the very first add into it
         * failed (e.g. bad source path), don't leave an empty/poisoned MPQ
         * behind - remove it so the next attempt starts clean rather than
         * silently reopening a file-less archive. */
        if (createdNew) {
            remove(archivePath);
        }
        fprintf(stderr, "mpqpack: SFileAddFileEx failed: %s\n", mpq_strerror(addErr));
        return 1;
    }

    if (!SFileCloseArchive(hMpq)) {
        fprintf(stderr, "mpqpack: SFileCloseArchive failed: %s\n", mpq_strerror(SErrGetLastError()));
        return 1;
    }

    printf("OK: %s -> %s [%s]\n", sourcePath, internalPath, archivePath);
    return 0;
}
