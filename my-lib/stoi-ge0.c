#include <limits.h>

// convert strings to integers >= 0
// return EINVAL if invalid, ERANGE if overflow
int
stoi_ge0(char const * str)
{
  if (str[0] == '\0')
	return -1;
  
  int current = 0;
  for (int i = 0; str[i]; i++)
	{
	  if (str[i] < '0' || str[i] > '9')
		return -1;
	  else if (i == 0)
		current = str[i] - '0';
	  else
		{
		  if (current > INT_MAX / 10)
			return -1;
		  current *= 10;
		  if (str[i] - '0' > INT_MAX - current)
			return -1;
		  current += str[i] - '0';
		}
	}
  
  return current;
}

/*
#include <assert.h>

int main ()
{
  assert(0 == stoi_ge0("0"));
  assert(9 == stoi_ge0("9"));
  assert(101 == stoi_ge0("101"));
  assert(121 == stoi_ge0("121"));
  static_assert(INT_MAX == 2147483647);
  assert(INT_MAX == stoi_ge0("2147483647"));
  assert(-1 == stoi_ge0("2147483648"));
  assert(-1 == stoi_ge0("hello"));
  assert(-1 == stoi_ge0("-"));
  assert(0 == stoi_ge0("000000"));
}
*/
