#ifndef L10N_H
#define L10N_H
/* Translations through locale.library catalogs (PROGDIR:Catalogs/<language>/Micropolis.catalog, or Locale:Catalogs/...).
 * T("English text") returns the translated text, or the English text itself when there is no catalog or no entry.
 * The catalog id of a text is a hash of the English text (see tools/l10n/l10n.py), so the code needs no id table.
 * N_("text") only marks a text in a table for the extractor; the translation is looked up where it is used: T(table[i]). */
void l10n_init(const char *language);       /* language: "english", "deutsch", "polski" ... or NULL / "" = the user's preferred language */
void l10n_exit(void);
const char *T(const char *english);
const char *l10n_language(void);            /* the language that is in use ("english" without a catalog) */
#define N_(s) s
#endif
