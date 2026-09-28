# A01 isolated native GUI smoke

Invariant: a local experimental launch owns a fresh data/tmp/cache namespace,
has a distinct app identity with no document/URL handlers, and runs behind the
same offline write restriction as the baseline CLI. The label option creates
separate test instances without overwriting prior evidence. No C++ or build
configuration changed in this milestone.

On 2026-09-28 the current macOS ARM64 Release application at source `1e0b5b98`
was rebuilt with the native test targets (exit 0, 379.209 seconds). The copied
core binary SHA is recorded in `evidence/A01-gui/bundle-manifest.json`; the bundle
is a local development artifact with a checkout Resources link, not a portable
or signed distribution. The preparation manifest correctly says GUI NOT_RUN;
the subsequent, separate `gui-observations.json` records the performed run.

The actual sandbox probe can write inside the isolated runtime, receives
EPERM/EACCES for a sibling write and a loopback socket connection, and observes
launcher rejection of a caller-supplied datadir. Native host GUI IPC remains
allowed; this is not proof that all external OS service brokers are sandboxed.
The installed `/Applications/OrcaSlicer.app` was not launched or modified.

The app was launched via its full development bundle path and configured only
inside `build/nonplanar-dev/gui-1e0b5b98/runtime`: Generic Klipper Printer,
Generic PLA, stealth mode enabled, network plugin installation unchecked. The
native file dialog imported the unchanged `flat_block.stl` fixture. Prepare
showed 30 x 20 x 4 mm, 96 triangles, volume 2400 mm3. Cmd-R completed slicing;
Preview showed 20 layers through Z=4.00, linear G-code and a 3m30s/1.70g estimate.
Screenshots retain those observations. No output was sent to a printer. Cmd-Q
closed the disposable project without saving; the process exited normally.

The automation AX query timed out after import while a screenshot and a process
sample showed the application alive in its idle event loop. AX subsequently
recovered. A coordinate hit-test on Slice plate failed, so its native Cmd-R
shortcut was used. These automation failures are not counted as application
crashes or skipped acceptance tests.

Fresh OFF/ZAA CLI captures for flat block, wedge and sphere all match pinned
stock motion and G-code with the existing timestamp normalization. Only the
documented additive resolved setting `nptop_mode: off` is allowed. Upstream
STL regression passed one case/eight assertions; the unchanged selected native
suite's latest 127/127 result is retained in B02-callback evidence. Commands,
exit codes, capture hashes and screenshot hashes are retained here.

Run commands are in the `.log.json` records. Reproduction after a build:

```sh
python3 scripts/nonplanar/prepare_dev_bundle.py --label gui-fresh
python3 scripts/nonplanar/validate_dev_bundle.py \
  --bundle build/nonplanar-dev/gui-fresh/NonplanarTopLab.app \
  --output build/nonplanar-evidence/gui-fresh-isolation.json
```

Then launch that exact bundle through its launcher, never its core directly.
Stock coexistence, preset-copy import, signed packaging, Linux/Windows GUI and
hybrid GUI remain NOT_RUN or NOT_IMPLEMENTED. This smoke test neither qualifies
the temporary printer preset nor completes Gate A's independent safety review.
