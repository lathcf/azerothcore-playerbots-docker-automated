/*
 * mpqread - extract one internal file from an MPQ to a local path, via
 * StormLib. Mirrors the open/read pattern of client-patch/mpqpack.c (same
 * locally-built StormLib, see build-mpqpack.sh).
 *
 * Usage: mpqread <archive.MPQ> <internal\path.dbc> <out_path>
 * Internal paths use backslashes, e.g. DBFilesClient\Spell.dbc
 *
 * Exit status: 0 on success, nonzero on any failure (message on stderr).
 */
#include <StormLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* StormLib on Linux reports errors via SErrGetLastError() (errno-space plus
 * a handful of StormLib-only codes) - there is no GetLastError() on this
 * platform, so use the same accessor mpqpack.c uses. */

int main(int argc, char** argv) {
    if (argc != 4) { fprintf(stderr, "usage: mpqread <mpq> <internal> <out>\n"); return 2; }

    const char* archivePath = argv[1];
    const char* internalPath = argv[2];
    const char* outPath = argv[3];

    HANDLE mpq = NULL, file = NULL;
    if (!SFileOpenArchive(archivePath, 0, MPQ_OPEN_READ_ONLY, &mpq)) {
        fprintf(stderr, "SFileOpenArchive(%s) failed: %u\n", archivePath, SErrGetLastError());
        return 1;
    }
    if (!SFileOpenFileEx(mpq, internalPath, 0, &file)) {
        fprintf(stderr, "SFileOpenFileEx(%s) failed: %u\n", internalPath, SErrGetLastError());
        SFileCloseArchive(mpq);
        return 1;
    }

    DWORD size = SFileGetFileSize(file, NULL);
    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf) {
        fprintf(stderr, "malloc(%u) failed\n", size);
        SFileCloseFile(file); SFileCloseArchive(mpq);
        return 1;
    }

    DWORD got = 0;
    if (!SFileReadFile(file, buf, size, &got, NULL) && got != size) {
        fprintf(stderr, "SFileReadFile failed: %u (got %u/%u)\n", SErrGetLastError(), got, size);
        free(buf); SFileCloseFile(file); SFileCloseArchive(mpq);
        return 1;
    }

    FILE* out = fopen(outPath, "wb");
    if (!out) {
        fprintf(stderr, "fopen(%s) failed\n", outPath);
        free(buf); SFileCloseFile(file); SFileCloseArchive(mpq);
        return 1;
    }
    fwrite(buf, 1, got, out);
    fclose(out);
    free(buf);

    SFileCloseFile(file);
    SFileCloseArchive(mpq);

    fprintf(stderr, "wrote %u bytes -> %s\n", got, outPath);
    return 0;
}
