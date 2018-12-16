/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.2.2                                      *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * Arduino Timezone Library Copyright (C) 2018 by Jack Christensen and  *
 * licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html   *
 *----------------------------------------------------------------------*/ 

/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.1                                        *
 * Gionata Boccalini                                                    *
 *  - 30/10/2016: Removed ARDUINO #if since it's not used in my         *
 *                development environment (ATTiny)                      *
 *  - 21/10/2017: Replaced Time.h Arduino library with time.h from      *
 *                avr-libc (to save space on the Tiny)                  *
 *  - 25/03/2017: Fixed bug in DST change time evaluation: the          *
 *                enumeration simply have to start from 0...            *
 *  - 24/11/2018: Added toLocal function with struct tm input/output    *
 *----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.3                                        *
 * Gionata Boccalini                                                    *
 *  - 16/12/2018: merged from upstream v1.2.2                           *
 *----------------------------------------------------------------------*/

#ifndef Timezone_h
#define Timezone_h
#include <Arduino.h> 
#include <time.h>              							                    // avr-libc built in time library.

// constants for time_t conversions
#define SECS_PER_MIN    60
#define SECS_PER_HOUR   3600
#define SECS_PER_DAY    86400

// convenient constants for TimeChangeRules
enum week_t {Last, First, Second, Third, Fourth}; 
enum dow_t {Sun, Mon, Tue, Wed, Thu, Fri, Sat};					            // avr-libc time.h: sunday is 0
enum month_t {Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};	// avr-libc time.h: months in [0, 11]

// structure to describe rules for when daylight/summer time begins,
// or when standard time begins.
struct TimeChangeRule
{
    char abbrev[6];    // five chars max
    uint8_t week;      // First, Second, Third, Fourth, or Last week of the month
    uint8_t dow;       // day of week, 0=Sun, 2=Mon, ... 6=Sat
    uint8_t month;     // 0=Jan, 1=Feb, ... 11=Dec
    uint8_t hour;      // 0-23
    int offset;        // offset from UTC in minutes
};
        
class Timezone
{
    public:
        Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart);
        Timezone(TimeChangeRule stdTime);
        Timezone(int address);
        time_t toLocal(time_t utc);
        time_t toLocal(time_t utc, TimeChangeRule **tcr);
        time_t toLocal(time_t utc, struct tm *tm_local, TimeChangeRule **tcr);
        time_t toLocal(struct tm *tm_utc, struct tm *tm_local, TimeChangeRule **tcr);
        time_t toUTC(time_t local);
        bool utcIsDST(time_t utc);
        bool locIsDST(time_t local);
        void setRules(TimeChangeRule dstStart, TimeChangeRule stdStart);
        void readRules(int address);
        void writeRules(int address);

    private:
        void calcTimeChanges(int yr);
        void initTimeChanges();
        time_t toTime_t(TimeChangeRule r, int yr);
        TimeChangeRule m_dst;   // rule for start of dst or summer time for any year
        TimeChangeRule m_std;   // rule for start of standard time for any year
        time_t m_dstUTC;        // dst start for given/current year, given in UTC
        struct tm m_tm_dstUTC;
        time_t m_stdUTC;        // std time start for given/current year, given in UTC
        time_t m_dstLoc;        // dst start for given/current year, given in local time
        struct tm m_tm_dstLoc;
        time_t m_stdLoc;        // std time start for given/current year, given in local time
};
#endif
