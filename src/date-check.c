#include "date-check.h"

bool isValidDate(int year, int month, int day)
{
  static int daysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  // NOTE: year is only checked for posive value
  bool validYear = year >= 0;
  bool validMonth = month >= 1 && month <= 12;
  bool isLeapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  bool validDay = day >= 1
                  && day <= (month == 2 ? (isLeapYear ? daysPerMonth[month - 1] + 1 : daysPerMonth[month - 1]) :
                             daysPerMonth[month - 1]);

  return validYear && validMonth && validDay;
}