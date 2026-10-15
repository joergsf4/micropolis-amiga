/* Catalog lookup, see l10n.h. */
#include <string.h>
#include <exec/types.h>
#include <libraries/locale.h>
#include <proto/exec.h>
#include <proto/locale.h>
#include <utility/tagitem.h>
#include "l10n.h"

struct LocaleBase *LocaleBase;
static struct Catalog *catalog;
static char lang_in_use[32] = "english";

static LONG text_id(const char *s)          /* 31 bit FNV-1a of the English text; tools/l10n/l10n.py uses the same */
{
    ULONG h = 2166136261UL;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
    return (LONG)(h & 0x7fffffffUL);
}

void l10n_init(const char *language)
{
    LocaleBase = (struct LocaleBase *)OpenLibrary((CONST_STRPTR)"locale.library", 38);
    if (!LocaleBase) return;
    if (language && !*language) language = NULL;
    if (language && !strcmp(language, "english")) return;           /* the built-in texts */
    if (language)
        catalog = OpenCatalog(NULL, (CONST_STRPTR)"Micropolis.catalog", OC_Language, (ULONG)language, OC_BuiltInLanguage, (ULONG)"english", TAG_DONE);
    else
        catalog = OpenCatalog(NULL, (CONST_STRPTR)"Micropolis.catalog", OC_BuiltInLanguage, (ULONG)"english", TAG_DONE);
    if (catalog && catalog->cat_Language) { strncpy(lang_in_use, (char *)catalog->cat_Language, sizeof lang_in_use - 1); lang_in_use[sizeof lang_in_use - 1] = 0; }
}

void l10n_exit(void)
{
    if (catalog) { CloseCatalog(catalog); catalog = NULL; }
    if (LocaleBase) { CloseLibrary((struct Library *)LocaleBase); LocaleBase = NULL; }
}

const char *T(const char *english)
{
    if (!catalog || !english || !*english) return english;
    return (const char *)GetCatalogStr(catalog, text_id(english), (CONST_STRPTR)english);
}

const char *l10n_language(void) { return lang_in_use; }
