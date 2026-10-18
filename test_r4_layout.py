from pathlib import Path
from datetime import date
root = Path(__file__).parent
cal = (root/'PIO/lib/ui/src/calendar.c').read_text()
ui = (root/'PIO/lib/ui/src/screens/ui_Screen1.c').read_text()
assert '&ui_font_Inter11R' in cal
assert 'const int step = (weeks == 6) ? 11 : 14;' in cal
assert 'CAL_ROW_H   14' in cal
assert 'lv_obj_set_style_bg_opa(cal_today, 0, 0);' in cal
assert 'cal_prev' not in cal and 'cal_next' not in cal
assert 'ui_font_Plex12' in ui
assert '270 + i * 8, 146, 4, 1' in ui
for year in range(2024, 2041):
 for month in range(1, 13):
  start=date(year,month,1).weekday()
  days=((date(year+1,1,1) if month==12 else date(year,month+1,1))-date(year,month,1)).days
  weeks=(start+days+6)//7
  step=11 if weeks==6 else 14
  last_top=47+(weeks-1)*step
  assert last_top+14<=118, (year,month,last_top)
  assert 1<=weeks<=6
print('R4 geometry checks PASS (2024-2040, 204 months)')
