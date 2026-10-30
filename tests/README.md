# Moon orientation regression test

`test_moon_orientation.py` renders the firmware's static moon frames on the
host (no flash required) for every month of the current year and compares the
illuminated side with timeanddate moon renderings.

The reference PNGs in `moon_reference_<current-year>/` contain four samples
per month for the current test year.  They are cached from
`https://www.timeanddate.com/scripts/moon.php` using the parameters computed by
`moon_model.py`.

To refresh the cached references, open the timeanddate moon page in a headed
Chrome with remote debugging enabled, then run:

```bash
google-chrome --remote-debugging-port=9227 \
  'https://www.timeanddate.com/moon/phases/china/beijing'
python tests/fetch_moon_references.py --port 9227
```

The normal test is offline and runs from `make test`; the test derives the
current year from the host date and looks for `moon_reference_<year>/`.  If
that directory is missing, run the fetch command above to create it.
