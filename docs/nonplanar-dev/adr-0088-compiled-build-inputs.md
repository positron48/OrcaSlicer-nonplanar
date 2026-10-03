# ADR-0088: library-owned build input inventory in native jobs

Status: bounded implementation; local verification passed. Full B01–B15 and
independent safety review remain open.

Invariant: a native job using `CompiledInputs` obtains its Software resource
from the running library's protected `compiled_build_inputs()` factory. A caller
cannot replace that resource with an imported declaration. Its exact bytes and
SHA-256 participate in the existing version-1 job fingerprint, candidate manifest
and report binding. This is an inventory identity, not a software certificate.

The CMake build hashes the actual contents of all files under `src`, `deps_src`,
`resources` and `cmake`; dependency CMake recipes; root CMake/version/build scripts;
and the pinned upstream lock. Build/download caches, `.git` and `.DS_Store` are
excluded. Relative paths and hashes are sorted. Declared compiler, configuration,
C++ flags, target definitions/options, generator and system context are included.
No timestamps or environment dump. The inventory generator runs every build,
including when a changed resource retains its mtime. Unchanged output preserves
the generated header's mtime; additions/removals trigger CMake's glob check.
Separate configuration headers and bounded raw-string parts preserve exact UTF-8
bytes without raising CMake's minimum version or adding a Python build dependency.

The private immutable C++ snapshot validates the compiled inventory's digest,
canonical JSON, schema and ordered file identities. Native jobs own that snapshot
and insert its exact Software resource within the existing count/byte and one
second capture limits. Cancellation and final native freshness checks remain.
An explicit duplicate Software resource or unknown capture mode rejects the new
attempt and invalidates its predecessor. OFF bypasses capture. Existing declared
diagnostic fixtures retain their version-1 bytes and have no protected snapshot.
Native body/lineage/departure/report tests opt into the protected mode.

No production profile/project schema migration: the existing Software resource
row carries a new name and actual digest. Old fingerprints remain different;
no normalization, proof reuse or promotion of an imported identity. This does
not add `Verified`, permit export, modify margins or change any geometry check.

The ledger samples files at build time and is compiled into this library. It
does not prove that every reused object was compiled from those bytes, especially
after source edits preserving timestamps or during concurrent builds/edits.
It does not resolve all per-source flags, transitive includes, installed dependency
archives, executable linkage, loaded plugins or runtime resource copies. Those
must be bound and checked before qualifying complete `software_identity`.
Consequently the fixed 17-check registry retains 4 PASS/RUN and 13 UNKNOWN/NOT_RUN;
software identity remains UNKNOWN/NOT_RUN and export remains BLOCK.

Positive and negative coverage includes content changes with preserved mtime,
actual executable inventory updates, stable repeated output, resource addition
and deletion, configurations, UTF-8/chunk reconstruction, missing/duplicate/
unsorted/traversal input rejection, protected C++ construction, declared-resource
overrides, exact job dependency encoding, cancellation and late native edits.
An independent Python oracle reconstructs the complete file inventory and checks
its exact linkage in native job reports; it does not call the CMake generator
or C++ serialization to obtain expected hashes.

GNU Make 3.81 can miss both recompilation and relinking when outputs change
within one second. An isolated object target supplies its exact CMake-generated
object paths. On inventory changes, Make builds invalidate that object and the
producer/transitive consumer artifacts; independent targets remain. Every removal
is restricted to the physical build tree, including paths with missing leaves
and symlinked ancestors. Ninja uses its normal generated-header dependencies.
The object unit inherits producer compilation dependencies except libnest2d,
which links back to libslic3r and is unused by the inventory. The existing static
cycle remains supported. MSVC explicitly uses UTF-8 for generated literals.

Final local verification: build exit 0; 8 generator tests; C++ focus 10 cases /
230 assertions; selected CTest 453 / 453 with zero failures or skips; complete
independent inventory and three native report bindings; existing context,
manifest and six report oracles; six strict fresh OFF/ZAA baseline pairs.
Full software identity, Linux/Windows, GUI/publication and physical qualification
remain open; the complete B objective remains in progress.
