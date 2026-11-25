#ifndef SIMCITY_IMPORT_H
#define SIMCITY_IMPORT_H
/* Loading a city: a Micropolis .cty file, or a city of the original SimCity (Amiga/Atari ST/Mac), which is the same data in a
 * MacBinary wrapper. Returns CITY_FAILED, CITY_LOADED or CITY_IMPORTED (then CityFileName is cleared, so that "Save City"
 * asks for a new .cty name instead of overwriting the original file, and the city name comes from the file's header). */
enum { CITY_FAILED, CITY_LOADED, CITY_IMPORTED };
int city_load_any(const char *path);
#endif
