# Exact evidence transport supplement

The first unpublished commit omitted twelve baseline .log files because the
repository-wide .gitignore excludes *.log. Original source-manifest and archived
bytes are preserved. Each omitted member is now carried in a deterministic
.log.gz file, with the exact uncompressed hash retained in delivery-manifest.json.
The manifest supplies fallback paths for these members; all other original
archive paths remain exact. No ignored docs or logs were force-added.

Raw Catch output has trailing blank lines. The complete artifact diff check
retains exit2 for those data files; the separately recorded authored13-file diff
check passes. Raw output must not be trimmed to make an artifact whitespace check
pass. This supplement changes no implementation, test, fixture or source hash.

Run verify.py to check committed Git membership, every original source/dependency/
binary/archive hash, and all lossless raw-to-archive mappings using only the
twelve explicit fallback paths. Rebuild binaries first on another machine;
this local check intentionally binds the recorded binaries instead of claiming
cross-platform reproducibility or qualified software identity.
