# בעיות אפשריות בהרצה ראשונה

מסמך זה מתאר **למה התוכנית עלולה לא לעבוד** בפעם הראשונה, לפי סדר ההרצה האמיתי של הקוד.

## הרצה תקינה (בסיס להשוואה)

```bash
cd /path/to/virus_detector   # חשוב: שורש הפרויקט
make
./build/bin/av_scanner help
./build/bin/av_scanner scan ./test_scan
```

כל הנתיבים בתוכנית הם **יחסיים ל־cwd** (תיקיית העבודה הנוכחית), לא למיקום הבינארי.

| נתיב בקוד | שימוש |
|-----------|--------|
| `config/signatures.txt` | חתימות |
| `config/exclude.txt` | החרגות משתמש |
| `runtime/logs/` | לוגים |
| `runtime/cache/cache.json` | מטמון |
| `runtime/resume/checkpoint.json` | Resume |
| `runtime/quarantine/` | הסגר |

---

# 1. שלב הבנייה (`make`)

### אין `g++` / Make
**סימפטום:** `g++: command not found` או `make: command not found`  
**למה:** הפרויקט דורש קומפיילר C++23 ו־Make.  
**מה לעשות:** להתקין `build-essential` (או מקבילה) / לפתוח ב־devcontainer.

### כשל קישור threads
**סימפטום:** undefined reference ל־`std::thread` / pthread  
**למה:** חסר `-pthread` בלינק (ב־Makefile כבר יש `LDFLAGS = -pthread`).  
**מה לעשות:** לבנות עם ה־Makefile של הפרויקט, לא עם פקודת `g++` ידנית בלי הדגל.

### דיסק case-insensitive / התנגשות שמות
**סימפטום:** בעיות מוזרות עם תיקיית `Scanner/` מול קובץ בינארי בשם דומה  
**למה:** לכן הבינארי נקרא `build/bin/av_scanner` ולא `scanner`.  
**מה לעשות:** לא לשנות את שם הפלט ל־`scanner` על מערכת כזו.

---

# 2. הרצה מתיקייה לא נכונה (הכי נפוץ)

### הרצה מתוך `build/bin/`
```bash
cd build/bin
./av_scanner scan /
```

**סימפטום:**
- `Could not initialize logger`
- או `Could not open signatures file`
- או `Could not load exclude file`

**למה:** התוכנית מחפשת `runtime/` ו־`config/` **מתחת ל־cwd**, לא ליד הבינארי.  
**מה לעשות:** תמיד להריץ משורש הפרויקט:

```bash
./build/bin/av_scanner scan <path>
```

---

# 3. Logger — נקודת עצירה ראשונה

סדר בקוד: Logger נוצר **לפני** כל דבר אחר.

### אין הרשאת כתיבה ל־`runtime/logs`
**סימפטום:** `Could not initialize logger` → exit code 1  
**למה:** `Logger` עושה `create_directories("runtime/logs")` ופותח קובץ חדש. אם אין הרשאה / filesystem read-only — נכשל.  
**מה לעשות:** לוודא שיש הרשאת כתיבה בשורש הפרויקט, או ליצור ידנית:

```bash
mkdir -p runtime/logs
```

### אין מקום בדיסק
**סימפטום:** אותו דבר — לא נפתח קובץ לוג  
**למה:** `ofstream` נכשל.  
**מה לעשות:** לפנות מקום בדיסק.

---

# 4. אתחול Scanner (`initialize`)

קורה רק בפקודות `scan` / `scan-all`.

## 4.1 חתימות — `config/signatures.txt`

| בעיה | סימפטום | למה |
|------|---------|-----|
| הקובץ לא קיים | `Could not open signatures file` | `SignatureManager::load` מחזיר `FileOpenFailed` — **שגיאה קריטית** |
| הקובץ ריק / רק הערות | `No valid signatures loaded` | אחרי סינון אין אף חתימה תקפה |
| אין הרשאת קריאה | אותו כשל פתיחה | Permission denied |

בלי חתימות — אין אוטומט — הסריקה לא מתחילה.

## 4.2 Exclude — `config/exclude.txt`

| בעיה | סימפטום | למה |
|------|---------|-----|
| הקובץ לא קיים | `Could not load exclude file` | גם זה **קריטי** כרגע |
| נתיבים יחסיים בקובץ | לא נטענים (נדלגים בשקט) | הקוד מקבל **רק absolute paths** |
| נתיבי exclude מהדוגמה לא רלוונטיים למכונה שלך | לא שובר, אבל לא מגן על runtime שלך | ב־`exclude.txt` מופיעים נתיבי `/workspace/virus_detector/...` — במחשב אחר צריך לעדכן |

**חשוב:** תיקיות מערכת (`/proc`, `/sys`, `/dev`, `/run`, `/tmp`) מוחרגות **בקוד**, לא רק בקובץ.

## 4.3 Checkpoint / Cache / Quarantine directories

| רכיב | נתיב | כשל קריטי? | מה קורה |
|------|------|------------|---------|
| Resume | `runtime/resume/` | כן | לא מצליח ליצור תיקייה → עצירה |
| Cache | `runtime/cache/` | לא | Warning + ממשיך עם cache ריק |
| Quarantine | `runtime/quarantine/files/` | כן | לא מצליח ליצור תיקיות → עצירה |

**למה Cache רך ו־Quarantine קשיח:** בלי cache אפשר לסרוק; בלי תיקיית הסגר אי אפשר לטפל בזדוניים לפי הדרישה.

### Cache JSON ישן בלי `file_size`
**סימפטום:** Warning על cache / cache ריק  
**למה:** הטעינה דורשת שדה `file_size`. קובץ ישן בלי השדה נכשל ב־parse.  
**מה לעשות:** למחוק `runtime/cache/cache.json` ולהמשיך (ייצור מחדש).

---

# 5. בעיות בפקודת הסריקה עצמה

## 5.1 `scan <path>` — הנתיב לא קיים
**סימפטום:** סריקה נכשלת / enumeration failed / failed > 0  
**למה:** `FileEnumerator` בודק `exists(root)` ומחזיר false.

## 5.2 אין הרשאות לקרוא את ה־root
**סימפטום:** שגיאות Access/Open directory בלוג; סיכום עם failed  
**למה:** filesystem עם `error_code` — לא exception, אבל הסריקה לא מצליחה.

## 5.3 `scan-all` (שורש `/`)
| בעיה | למה זה נשבר / איטי |
|------|---------------------|
| הרבה Permission denied | קבצים של root/משתמשים אחרים |
| סריקה ארוכה מאוד | כל הדיסק + ThreadPool |
| סיכון לולאות פחות רלוונטי | symlinks מדולגים בקוד |
| Exclude של runtime חייב absolute נכון | אחרת עלול לסרוק גם את `runtime/` של עצמו |

## 5.4 נתיב הוא symlink
**סימפטום:** Discovered=0 / Excluded גדל; "Skipping symbolic link" בלוג  
**למה:** במכוון — לא נכנסים ל־symbolic links.

## 5.5 קבצים ללא הרשאת קריאה בתוך העץ
**סימפטום:** `failed` עולה; בלוג `[FileOpenFailed] ... Permission denied`  
**למה:** שגיאה **מקומית** — ממשיכים לקבצים הבאים. לא עוצר את כל הסריקה.

---

# 6. זיהוי / הסגר — למה "לא מוצא וירוסים"

### החתימות לא מופיעות בקובץ
החתימות ב־`config/signatures.txt` כרגע הן מחרוזות קצרות כמו `Trojan`, `Virus`.  
קובץ בדיקה בשם `virus.txt` **לא בהכרח** ייתפס אם התוכן לא מכיל את המחרוזת עצמה.

### Cache hit על קובץ ששינית רק בתוכן באותו גודל ואותו mtime
נדיר, אבל אם גם `mtime` וגם `file_size` לא השתנו — cache יחשוב שהקובץ נקי.  
בפועל רוב העורכים משנים לפחות אחד מהם.

### Quarantine נכשל לקובץ בודד
**סימפטום:** הודעה על מסך + `failed++`  
**למה:** אין מקום / אין הרשאה להעביר / יעד תפוס (פחות סביר אחרי UniqueDestination).  
הסריקה ממשיכה.

---

# 7. Resume — למה "לא ממשיך מאיפה שעצרתי"

| מצב | התנהגות |
|-----|---------|
| אין `checkpoint.json` או `status=completed` | סריקה חדשה מההתחלה |
| `status=running` אבל `root` שונה | לא יעשה resume לאותו checkpoint |
| Crash באמצע | ממשיך מ־`next_unfinished_path` (כולל הקובץ עצמו) |
| Checkpoint פגום / JSON לא תקין | נחשב כאין resume תקף |

שמירת checkpoint מתבצעת בעיקר כש־**frontier** (`next_unfinished_path`) משתנה, ועוד flush בסוף.

---

# 8. פקודות Quarantine בלי סריקה

`restore` / `delete` / `quarantine-list` מאתחלים רק Quarantine (לא Scanner מלא).

| בעיה | סימפטום |
|------|---------|
| אין תיקיית quarantine / אין הרשאה | `Could not initialize quarantine` |
| ID לא קיים | `Could not restore/delete file` |
| ב־restore היעד המקורי כבר קיים | נכשל כדי לא לדרוס קובץ |

---

# 9. ThreadPool / ביצועים (פחות "לא עובד", יותר "נתקע")

| תופעה | הסבר |
|-------|------|
| סריקה נראית תקועה על תיקייה גדולה | Enumerator מחכה כשהתור מלא (`space_cv`) — זה תקין |
| מעט ניצול CPU | ברירת מחדל: `worker_count=0` → `hardware_concurrency()` |
| תהליך לא נסגר | Destructor של ThreadPool עושה join; אמור להסתיים אחרי `wait()` |

---

# 10. Checklist מהיר לפני הרצה ראשונה

```text
[ ] נמצאים בשורש הפרויקט (יש config/ ו-Makefile)
[ ] make עבר בלי שגיאות
[ ] קיימים config/signatures.txt ו-config/exclude.txt
[ ] יש לפחות חתימה אחת לא-מוערת ב-signatures.txt
[ ] יש הרשאת כתיבה ליצירת runtime/
[ ] מריצים: ./build/bin/av_scanner scan <path_שקיים>
[ ] לא מריצים מתוך build/bin/
[ ] ב-exclude.txt נתיבים מוחלטים מתאימים למכונה שלך (אופציונלי אבל מומלץ)
```

---

# 11. מפת כשלים לפי הודעה

| הודעה / סימפטום | סיבה הסבירה ביותר |
|-----------------|-------------------|
| `Could not initialize logger` | cwd לא נכון / אין כתיבה ל־`runtime/logs` |
| `Could not open signatures file` | חסר `config/signatures.txt` ביחס ל־cwd |
| `No valid signatures loaded` | קובץ חתימות ריק או רק `#` |
| `Could not load exclude file` | חסר `config/exclude.txt` |
| `Could not initialize checkpoint` | אין כתיבה ל־`runtime/resume` |
| `Could not initialize quarantine` | אין כתיבה ל־`runtime/quarantine` |
| Cache warning + ממשיך | JSON cache פגום/ישן — לא קריטי |
| `Scanner is not initialized` | באג שימוש פנימי / initialize נכשל ולא נבדק |
| `Unknown or invalid command` | חסר ארגומנט ל־`scan`/`restore`/`delete` |
| `Permission denied` בלוג | קובץ/תיקייה בלי הרשאה — מקומי |
| `Skipping symbolic link` | התנהגות מכוונת |
| Discovered > 0 אבל Malicious = 0 | אין התאמת מחרוזת מהחתימות בתוכן הקבצים |

---

# 12. מה *לא* אמור לשבור הרצה ראשונה

- חוסר ב־`test_scan/` (לא חובה; רק נוח לבדיקה)
- Cache ריק או חסר
- Checkpoint חסר
- תיקיות `runtime/*` חסרות — אמורות להיווצר אוטומטית אם יש הרשאות
- עדכון `signatures.txt` בלי recompile — זה מותר ומתוכנן

---

אם משהו נכשל: להסתכל קודם על **השורה הראשונה של השגיאה במסך**, ואז על קובץ הלוג האחרון ב־`runtime/logs/`. שם מופיע בדרך כלל הקוד (`[FileOpenFailed]`, `[CheckpointFailed]` וכו') והנתיב המדויק.
