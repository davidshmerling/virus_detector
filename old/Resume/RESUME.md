# מנגנון Resume — איך הוא עובד

## הגדרה

קובץ נחשב **completed** כשכל הפעולות שמשפיעות על נכונות הסריקה שלו כבר הושלמו.

הקאש הוא אופטימיזציה בלבד — אין צורך לחכות ל־SQLite COMMIT לפני `markCompleted`.

```text
Resume → מונע דילוג על עבודה שלא הסתיימה
Cache  → מונע סריקה חוזרת בריצות עתידיות
```

---

## מתי קובץ completed

| תוצאה | מתי נחשב completed |
|--------|---------------------|
| Cache hit | מיד |
| נקי | אחרי שהסריקה הסתיימה (גם אם העדכון לקאש עוד ב־buffer / בתור / לפני COMMIT) |
| מזיק | רק אחרי שההעברה להסגר הצליחה |
| מזיק וההסגר נכשל | **לא** completed — Resume ינסה שוב |
| שגיאת פתיחה/קריאה | completed + failed (כדי שה־frontier לא ייתקע) |

---

## אחריות הרכיבים

| רכיב | אחריות |
|------|--------|
| **Enumerator יחיד** | DFS לקסיקוגרפי ממוין; רושם משימות ב־`registerTask` בסדר גלובלי |
| **Worker** | סורק / מסגיר; קורא ל־`markCompleted` לפי ה־verdict |
| **CacheWriter** | מעדכן map + SQLite; **לא** קשור ל־progress |
| **ProgressTracker** | מחזיק `std::set unfinished_paths_`; checkpoint = `*unfinished.begin()` |

---

## זרימה

```text
Enumerator (יחיד, DFS ממוין)
  → registerTask(path)     # מוסיף ל-unfinished
  → ThreadPool::enqueue

Worker
  ├─ cache hit
  │     → markCompleted
  ├─ נקי
  │     → CacheUpdate (async)
  │     → markCompleted
  ├─ מזיק + הסגר הצליח
  │     → markCompleted
  ├─ מזיק + הסגר נכשל
  │     → לא markCompleted
  └─ שגיאת I/O
        → markCompleted (+ failed ב-summary)

finishScan:
  markEnumerationFinished
  thread_pool.wait
  cache_manager.flush      ← best-effort
  progress_tracker.flush
```

---

## Checkpoint = מינימום ב־unfinished

בזיכרון:

```text
std::set<std::string> unfinished_paths_;
checkpoint = *unfinished_paths_.begin();
```

לא המינימום מבין מה שהסתיים — אלא **המינימום הלקסיקוגרפי מבין מה שעדיין לא הסתיים**.

### דוגמה

```text
נשלחו: a b c d e
הסתיימו: a, c, e
עדיין פתוחים: b, d

unfinished = {b, d}
checkpoint = b
```

תנאי קריטי: Enumerator יחיד שמגלה בסדר לקסיקוגרפי גלובלי.

### unfinished ריק באמצע סריקה

```text
unfinished ריק && enumeration_finished == false
```

עדיין לא `completed`. שומרים את ה־frontier הקודם (קריסה במקרה זה תסרוק מחדש לכל היותר קובץ אחד).

רק כאשר:

```text
unfinished ריק && enumeration_finished == true
```

נקבע `status = completed`.

---

## מבנה checkpoint

`runtime/resume/checkpoint.json`:

| שדה | משמעות |
|-----|--------|
| `root` | שורש הסריקה |
| `status` | `running` / `completed` |
| `next_unfinished_path` | `min(unfinished_paths_)` |

---

## אלגוריתם walk ב־resume

בהינתן `next_unfinished_path`, DFS ממוין חד־טרדי:

| שם ילד | פעולה |
|--------|--------|
| `<` target | דילוג |
| `==` target | המשך לעומק / rescan של הקובץ |
| `>` target | DFS מלא |
