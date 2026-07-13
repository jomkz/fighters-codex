#pragma once
#include <cstddef>
#include <cstdint>

// PKWare DCL ("Blast") decompressor — port of Mark Adler's blast.c
// Verified against PKWDCL.DLL and FA game files (BALTIC.TXT, A1.INF, SHWPILOT.TXT).
//
// EA wrapper variant: input begins with a 4-byte LE decompressed-size prefix,
// followed by the raw PKWARE DCL stream (litmode byte + dictbits byte + bitstream).
//
// Returns number of bytes written to out, or -1 on error.
// out_capacity must be >= the expected decompressed size.

namespace fx {

int blast_decompress_ea(const uint8_t* in, size_t in_size,
                        uint8_t* out, size_t out_capacity);

// Raw PKWARE DCL stream (no EA size prefix).
int blast_decompress(const uint8_t* in, size_t in_size,
                     uint8_t* out, size_t out_capacity);

} // namespace fx
