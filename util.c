#include <ctype.h>
#include <string.h>

void str_to_upper(char *s)
{
    while (*s)
    {
        *s = toupper((unsigned char)*s);
        s++;
    }
}

void str_trim(char *s)
{
    int len = strlen(s);

    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' '))
    {
        s[len-1] = 0;
        len--;
    }
}
