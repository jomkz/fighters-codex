# TODO

## Tests

Catch2 test suite lives in `tests/`. Run with `ctest --test-dir build -C Release`.

### Modules needing test coverage

#### blast.cpp — decompressor (highest priority)
- Round-trip: compress a known payload with a reference tool, decompress with `blast_decompress_ea`, compare bytes
- Fixture-based: check a real DCL-compressed entry extracted from an FA `.LIB` (flags==4) against its known plaintext
- Error paths: truncated input, corrupt stream, declared size mismatch

#### pal.cpp — palette codec
- Round-trip: encode a 256-entry palette → decode → compare entries
- Known-value: verify a hand-crafted palette byte sequence decodes to expected RGB triples
- Edge cases: 0-entry palette, single-entry palette

#### pic.cpp — PIC image codec
- Round-trip: encode a small synthetic image → decode → pixel-compare
- Known dimensions: check width/height parsed from a hand-crafted header
- Error paths: bad magic, truncated row data

#### cb8.cpp — CB8 codec
- Round-trip encode/decode
- Verify output dimensions match input

#### seq.cpp — SEQ animation parser
- Frame count matches fixture header
- Per-frame data extraction correctness
- Error paths: bad header, out-of-bounds frame index

#### brf.cpp — BRF calibration
- Output within tolerance for known input/expected pairs
- Edge: all-zero input, single-sample input

#### plt.cpp — PLT format
- Round-trip parse/serialize
- Field values match known fixture

#### mission.cpp — mission file parser
- Parse a minimal synthetic mission blob, check field values
- Error paths: missing required fields, truncated input

#### sms.cpp / t2.cpp / sh.cpp — smaller formats
- Basic round-trip for each
- At least one error-path test per format

#### ot.cpp — OT format
- Parse known header, verify entry offsets
- Round-trip build → read → extract

#### fnt.cpp — font format
- Glyph count matches header
- Glyph data extraction by index

#### hud.cpp / lay.cpp / inf.cpp — HUD/layout/INF parsers
- Minimal parse + field-value checks for each

### Infrastructure
- Add a `tests/fixtures/` directory with small captured binary samples from a real FA install for formats that are impractical to synthesise (blast compressed streams, FNT files, etc.)
- Wire `ctest` into CI when a pipeline is set up
