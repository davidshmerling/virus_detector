# מנגנון Resume — איך הוא עובד

## הגדרה

ה־checkpoint לא אומר "עד כאן הסריקה הסתיימה", אלא:

> עד כאן גם הסריקה הסתיימה **וגם** התוצאה נשמרה בצורה durable (SQLite COMMIT / תוצאה שכבר עמידה).

לכן תמיד מתקיים:

```text
checkpoint  <=  cache (SQLite)
```

ה־checkpoint לעולם לא "לפני" הקאש.

---

## אחריות הרכיבים

| רכיב | אחריות |
|------|--------|
| **Worker** | סריקת קבצים בלבד; שולח upsert / complete ל־CacheWriter |
| **CacheWriter** | עדכון `unordered_map`, כתיבת SQLite, ורק אחרי COMMIT — `markCompleted` |
| **ProgressTracker** | מחזיק `unfinished_paths_` ושומר frontier; לא מחליט מתי קובץ "באמת הושלם" |

---

## זרימה

```text
Worker
  ├─ cache hit / malicious / error
  │     → notifyDurableComplete(relative)
  │           → CacheWriter queue
  │           → ProgressTracker::markCompleted
  │
  └─ clean
        → batch (10) → CacheWriter queue
              → update unordered_map
              → SQLite COMMIT (כל ~100 dirty)
              → ProgressTracker::markCompleted(...)   ← רק אחרי COMMIT
```

`finishScan`:

```text
markEnumerationFinished
thread_pool.wait
cache_manager.flush     ← COMMIT + markCompleted
progress_tracker.flush  ← שמירת frontier סופית
```

---

## מה קורה בקריסה

נסרקו `A..H`, אבל SQLite שמר רק `A..D`:

```text
checkpoint = D
SQLite     = A..D
```

אחרי Resume נסרקים שוב `E..H`. לא מפספסים כלום.

לפני השינוי היה אפשרי:

```text
checkpoint = H
SQLite     = D   ← חוסר עקביות
```

---

## מבנה checkpoint

`runtime/resume/checkpoint.json`:

| שדה | משמעות |
|-----|--------|
| `root` | שורש הסריקה |
| `status` | `running` / `completed` |
| `next_unfinished_path` | `min(unfinished_paths_)` — הגבול לקסיקוגרפי |

בזיכרון: `set` של משימות פתוחות. נשמר לדיסק כשהגבול זז (batch) + flush בסוף.

---

## אלגוריתם walk ב־resume

בהינתן `next_unfinished_path`, DFS ממוין חד־טרדי:

| שם ילד | פעולה |
|--------|--------|
| `<` target | דילוג |
| `==` target | המשך לעומק / rescan של הקובץ |
| `>` target | DFS מלא |

---

## מגבלה: enumeration מקבילי

ה־frontier עדיין מניח שגילוי קבצים הוא בסדר לקסיקוגרפי גלובלי.
חלוקת ילדי־root בין תרדים עלולה לקדם frontier ל־`z/...` בזמן ש־`a/` עוד לא נגלה.

סנכרון עם הקאש פותר חוסר־עקביות cache/checkpoint; **לא** פותר לבד את באג הסדר המקבילי.
