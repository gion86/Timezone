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
 *  - 24/11/2018: Added toLocal function with struct tm input/output    *
 *----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.3                                        *
 * Gionata Boccalini                                                    *
 *  - 16/12/2018: merged from upstream v1.2.2                           *
 *----------------------------------------------------------------------*/

#include "Timezone.h"

#ifdef __AVR__
    #include <avr/eeprom.h>
#endif

/*----------------------------------------------------------------------*
 * Create a Timezone object from the given time change rules.           *
 *----------------------------------------------------------------------*/
Timezone::Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart)
    : m_dst(dstStart), m_std(stdStart)
{
    initTimeChanges();
}

/*----------------------------------------------------------------------*
 * Create a Timezone object for a zone that does not observe            *
 * daylight time.                                                       *
 *----------------------------------------------------------------------*/
Timezone::Timezone(TimeChangeRule stdTime)
    : m_dst(stdTime), m_std(stdTime)
{
    initTimeChanges();
}

#ifdef __AVR__
/*----------------------------------------------------------------------*
 * Create a Timezone object from time change rules stored in EEPROM     *
 * at the given address.                                                *
 *----------------------------------------------------------------------*/
Timezone::Timezone(int address)
{
    readRules(address);
}
#endif

/*----------------------------------------------------------------------*
 * Convert the given UTC time to local time, standard or                *
 * daylight time, as appropriate.                                       *
 *----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc)
{
    struct tm utc_tm;

    // Conversion from time_t to struct tm to compare years
    gmtime_r(&utc, &utc_tm);

    // Recalculate the time change points if needed
    if (utc_tm.tm_year != m_tm_dstUTC.tm_year) calcTimeChanges(utc_tm.tm_year);

    if (utcIsDST(utc))
        return utc + m_dst.offset * SECS_PER_MIN;
    else
        return utc + m_std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Convert the given UTC time to local time, standard or                *
 * daylight time, as appropriate, and return a pointer to the time      *
 * change rule used to do the conversion. The caller must take care     *
 * not to alter this rule.                                              *
 *----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc, TimeChangeRule **tcr)
{
    struct tm utc_tm;

    // Conversion from time_t to struct tm to compare years
    gmtime_r(&utc, &utc_tm);

    // Recalculate the time change points if needed
    if (utc_tm.tm_year != m_tm_dstUTC.tm_year) calcTimeChanges(utc_tm.tm_year);

    if (utcIsDST(utc)) {
        *tcr = &m_dst;
        return utc + m_dst.offset * SECS_PER_MIN;
    }
    else {
        *tcr = &m_std;
        return utc + m_std.offset * SECS_PER_MIN;
    }
}

time_t Timezone::toLocal(time_t utc, struct tm *tm_local, TimeChangeRule **tcr)
{
    memset((void*) tm_local, 0, sizeof(*tm_local));
    time_t local = toLocal(utc, tcr);
    gmtime_r(&local, tm_local);

    return local;
}

time_t Timezone::toLocal(struct tm *tm_utc, struct tm *tm_local, TimeChangeRule **tcr)
{
    memset((void*) tm_local, 0, sizeof(*tm_local));
    time_t utc = mk_gmtime(tm_utc);
    time_t local = toLocal(utc, tcr);
    gmtime_r(&local, tm_local);

    return local;
}

/*----------------------------------------------------------------------*
 * Convert the given local time to UTC time.                            *
 *                                                                      *
 * WARNING:                                                             *
 * This function is provided for completeness, but should seldom be     *
 * needed and should be used sparingly and carefully.                   *
 *                                                                      *
 * Ambiguous situations occur after the Standard-to-DST and the         *
 * DST-to-Standard time transitions. When changing to DST, there is     *
 * one hour of local time that does not exist, since the clock moves    *
 * forward one hour. Similarly, when changing to standard time, there   *
 * is one hour of local times that occur twice since the clock moves    *
 * back one hour.                                                       *
 *                                                                      *
 * This function does not test whether it is passed an erroneous time   *
 * value during the Local -> DST transition that does not exist.        *
 * If passed such a time, an incorrect UTC time value will be returned. *
 *                                                                      *
 * If passed a local time value during the DST -> Local transition      *
 * that occurs twice, it will be treated as the earlier time, i.e.      *
 * the time that occurs before the transition.                          *
 *                                                                      *
 * Calling this function with local times during a transition interval  *
 * should be avoided!                                                   *
 *----------------------------------------------------------------------*/
time_t Timezone::toUTC(time_t local)
{
    struct tm tm_local;

    // Conversion from time_t to struct tm to compare years
    gmtime_r(&local, &tm_local);

    // Recalculate the time change points if needed
    if (tm_local.tm_year != m_tm_dstLoc.tm_year) calcTimeChanges(tm_local.tm_year);

    if (locIsDST(local))
        return local - m_dst.offset * SECS_PER_MIN;
    else
        return local - m_std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Determine whether the given UTC time_t is within the DST interval    *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
bool Timezone::utcIsDST(time_t utc)
{
    struct tm utc_tm;

    // Conversion from time_t to struct tm to compare years
    gmtime_r(&utc, &utc_tm);

    // Recalculate the time change points if needed
    if (utc_tm.tm_year != m_tm_dstUTC.tm_year) calcTimeChanges(utc_tm.tm_year);

    if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
        return false;
    else if (m_stdUTC > m_dstUTC)   // northern hemisphere
        return (utc >= m_dstUTC && utc < m_stdUTC);
    else                            // southern hemisphere
        return !(utc >= m_stdUTC && utc < m_dstUTC);
}

/*----------------------------------------------------------------------*
 * Determine whether the given UTC struct tm* is within the DST         *
 * interval or the Standard time interval.                              *
 *----------------------------------------------------------------------*/
bool Timezone::utcIsDST(struct tm* tm_utc)
{
    time_t utc;

    // Conversion from struct tm to time_t to compare times
    utc = mk_gmtime(tm_utc);

    // Recalculate the time change points if needed
    if (tm_utc->tm_year != m_tm_dstUTC.tm_year) calcTimeChanges(tm_utc->tm_year);

    if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
        return false;
    else if (m_stdUTC > m_dstUTC)   // northern hemisphere
        return (utc >= m_dstUTC && utc < m_stdUTC);
    else                            // southern hemisphere
        return !(utc >= m_stdUTC && utc < m_dstUTC);
}

/*----------------------------------------------------------------------*
 * Returns the UTC DST offset in minutes, retrieved from                *
 * DST TimeChangeRule, if UTC is DST, or Standard TimeChangeRule.       *
 *----------------------------------------------------------------------*/
int Timezone::getUTCDSTOffset(struct tm* tm_utc)
{
  if (utcIsDST(tm_utc))
      return m_dst.offset;
  else
      return m_std.offset;
}

/*----------------------------------------------------------------------*
 * Determine whether the given Local time_t is within the DST interval  *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
bool Timezone::locIsDST(time_t local)
{
    struct tm tm_local;

    // Conversion from time_t to struct tm to compare years
    gmtime_r(&local, &tm_local);

    // Recalculate the time change points if needed
    if (tm_local.tm_year != m_tm_dstLoc.tm_year) calcTimeChanges(tm_local.tm_year);

    if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
        return false;
    else if (m_stdLoc > m_dstLoc)   // northern hemisphere
        return (local >= m_dstLoc && local < m_stdLoc);
    else                            // southern hemisphere
        return !(local >= m_stdLoc && local < m_dstLoc);
}

/*----------------------------------------------------------------------*
 * Determine whether the given Local struct tm* is within the DST       *
 * interval or the Standard time interval.                              *
 *----------------------------------------------------------------------*/
bool Timezone::locIsDST(struct tm* tm_local)
{
    time_t local;

    // Conversion from struct tm to time_t to compare times
    local = mk_gmtime(tm_local);

    // Recalculate the time change points if needed
    if (tm_local->tm_year != m_tm_dstLoc.tm_year) calcTimeChanges(tm_local->tm_year);

    if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
        return false;
    else if (m_stdLoc > m_dstLoc)   // northern hemisphere
        return (local >= m_dstLoc && local < m_stdLoc);
    else                            // southern hemisphere
        return !(local >= m_stdLoc && local < m_dstLoc);
}

/*----------------------------------------------------------------------*
 * Returns the local DST offset in minutes, retrieved from              *
 * DST TimeChangeRule, if local is DST, or Standard TimeChangeRule.     *
 *----------------------------------------------------------------------*/
int Timezone::getLocalDSTOffset(struct tm* tm_local) {

    if (locIsDST(tm_local))
        return m_dst.offset;
    else
        return m_std.offset;
}

/*----------------------------------------------------------------------*
 * Calculate the DST and standard time change points for the given      *
 * year as local and UTC time_t values.                                 *
 *----------------------------------------------------------------------*/
void Timezone::calcTimeChanges(int yr)
{
    m_dstLoc = toTime_t(m_dst, yr);
    gmtime_r(&m_dstLoc, &m_tm_dstLoc);
    m_stdLoc = toTime_t(m_std, yr);
    m_dstUTC = m_dstLoc - m_std.offset * SECS_PER_MIN;
    gmtime_r(&m_dstUTC, &m_tm_dstUTC);
    m_stdUTC = m_stdLoc - m_dst.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Initialize the DST and standard time change points.                  *
 *----------------------------------------------------------------------*/
void Timezone::initTimeChanges()
{
    m_dstLoc = 0;
    gmtime_r(&m_dstLoc, &m_tm_dstLoc);
    m_stdLoc = 0;
    m_dstUTC = 0;
    gmtime_r(&m_dstUTC, &m_tm_dstUTC);
    m_stdUTC = 0;
}

/*----------------------------------------------------------------------*
 * Convert the given time change rule to a time_t value                 *
 * for the given year.                                                  *
 *----------------------------------------------------------------------*/
time_t Timezone::toTime_t(TimeChangeRule r, int yr)
{
    struct tm tm, tm_wday;
    time_t t;
    uint8_t m = r.month;     // temp copies of r.month and r.week
    uint8_t w = r.week;
    if (w == 0)              // is this a "Last week" rule?
    {
        if (++m > 11)        // yes, for "Last", go to the next month: avr-libc time.h: months in [0, 11]
        {
            m = 1;
            ++yr;
        }
        w = 1;               // and treat as first week of next month, subtract 7 days later
    }

    tm.tm_hour = r.hour;
    tm.tm_min= 0;
    tm.tm_sec = 0;
    tm.tm_mday = 1;
    tm.tm_mon = m;
    tm.tm_year = yr;         // avr-libc time.h: years since 1900 + y2k epoch difference (2000 - 1970)

    t = mk_gmtime(&tm);      // calculate first day of the month, or for "Last" rules, first day of the next month

    gmtime_r(&t, &tm_wday);  // conversion from time_t to struct tm to have week day

    // add offset from the first of the month to r.dow, and offset for the given week: weekday in [0, 6]
    t += ((r.dow - tm_wday.tm_wday + 7) % 7 + (w - 1) * 7) * SECS_PER_DAY;
    // back up a week if this is a "Last" rule
    if (r.week == 0) t -= 7 * SECS_PER_DAY;
    return t;
}

/*----------------------------------------------------------------------*
 * Read or update the daylight and standard time rules from RAM.        *
 *----------------------------------------------------------------------*/
void Timezone::setRules(TimeChangeRule dstStart, TimeChangeRule stdStart)
{
    m_dst = dstStart;
    m_std = stdStart;
    initTimeChanges();  // force calcTimeChanges() at next conversion call
}

#ifdef __AVR__
/*----------------------------------------------------------------------*
 * Read the daylight and standard time rules from EEPROM at             *
 * the given address.                                                   *
 *----------------------------------------------------------------------*/
void Timezone::readRules(int address)
{
    eeprom_read_block((void *) &m_dst, (void *) address, sizeof(m_dst));
    address += sizeof(m_dst);
    eeprom_read_block((void *) &m_std, (void *) address, sizeof(m_std));
    initTimeChanges();  // force calcTimeChanges() at next conversion call
}

/*----------------------------------------------------------------------*
 * Write the daylight and standard time rules to EEPROM at              *
 * the given address.                                                   *
 *----------------------------------------------------------------------*/
void Timezone::writeRules(int address)
{
    eeprom_write_block((void *) &m_dst, (void *) address, sizeof(m_dst));
    address += sizeof(m_dst);
    eeprom_write_block((void *) &m_std, (void *) address, sizeof(m_std));
}
#endif
