# Moon orientation regression test

`test_moon_orientation.py` renders the firmware's static moon frames on the
host (no flash required) for a complete lunation and compares the illuminated
side with timeanddate moon renderings.

The reference PNGs in `moon_reference/` cover one lunation, while
`moon_reference_2026/` covers four samples per month for all of 2026.  They are cached from
`https://www.timeanddate.com/scripts/moon.php` using the parameters computed by
`moon_model.py`.  Two additional images in the set are live page captures
(2026-10-20 and 2026-10-30) used as independent checks.

To refresh the cached references, open the timeanddate moon page in a headed
Chrome with remote debugging enabled, then run:

```bash
google-chrome --remote-debugging-port=9227 \
  'https://www.timeanddate.com/moon/phases/china/beijing'
python tests/fetch_moon_references.py --port 9227
```

The normal test is offline and runs from `make test`.
