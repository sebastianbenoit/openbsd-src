/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: time.c,v 1.7 2020/02/16 08:06:37 florian Exp $ */

/*! \file */



#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#include <sys/time.h>	/* Required for struct timeval on some platforms. */


#include <string.h>
#include <isc/time.h>

#include <isc/util.h>

#define NS_PER_S	1000000000	/*%< Nanoseconds per second. */
#define NS_PER_US	1000		/*%< Nanoseconds per microsecond. */

/*
 * All of the INSIST()s checks of nanoseconds < NS_PER_S are for
 * consistency checking of the type. In lieu of magic numbers, it
 * is the best we've got.  The check is only performed on functions which
 * need an initialized type.
 */

/*%
 *** Intervals
 ***/

static const interval_t zero_interval = { 0, 0 };
const interval_t * const interval_zero = &zero_interval;

void
interval_set(interval_t *i,
		 unsigned int seconds, unsigned int nanoseconds)
{
	REQUIRE(i != NULL);
	REQUIRE(nanoseconds < NS_PER_S);

	i->seconds = seconds;
	i->nanoseconds = nanoseconds;
}

isc_boolean_t
interval_iszero(const interval_t *i) {
	REQUIRE(i != NULL);
	INSIST(i->nanoseconds < NS_PER_S);

	if (i->seconds == 0 && i->nanoseconds == 0)
		return (ISC_TRUE);

	return (ISC_FALSE);
}


/***
 *** Absolute Times
 ***/

void
isc_time_set(isc_time_t *t, unsigned int seconds, unsigned int nanoseconds) {
	REQUIRE(t != NULL);
	REQUIRE(nanoseconds < NS_PER_S);

	t->seconds = seconds;
	t->nanoseconds = nanoseconds;
}

void
isc_time_settoepoch(isc_time_t *t) {
	REQUIRE(t != NULL);

	t->seconds = 0;
	t->nanoseconds = 0;
}

isc_boolean_t
isc_time_isepoch(const isc_time_t *t) {
	REQUIRE(t != NULL);
	INSIST(t->nanoseconds < NS_PER_S);

	if (t->seconds == 0 && t->nanoseconds == 0)
		return (ISC_TRUE);

	return (ISC_FALSE);
}


isc_result_t
isc_time_now(isc_time_t *t) {
	struct timeval tv;

	REQUIRE(t != NULL);

	if (gettimeofday(&tv, NULL) == -1) {
		UNEXPECTED_ERROR(__FILE__, __LINE__, "%s", strerror(errno));
		return (ISC_R_UNEXPECTED);
	}

	/*
	 * Ensure the tv_sec value fits in t->seconds.
	 */
	if (sizeof(tv.tv_sec) > sizeof(t->seconds) &&
	    ((tv.tv_sec | (unsigned int)-1) ^ (unsigned int)-1) != 0U)
		return (ISC_R_RANGE);

	t->seconds = tv.tv_sec;
	t->nanoseconds = tv.tv_usec * NS_PER_US;

	return (ISC_R_SUCCESS);
}

int
isc_time_compare(const isc_time_t *t1, const isc_time_t *t2) {
	REQUIRE(t1 != NULL && t2 != NULL);
	INSIST(t1->nanoseconds < NS_PER_S && t2->nanoseconds < NS_PER_S);

	if (t1->seconds < t2->seconds)
		return (-1);
	if (t1->seconds > t2->seconds)
		return (1);
	if (t1->nanoseconds < t2->nanoseconds)
		return (-1);
	if (t1->nanoseconds > t2->nanoseconds)
		return (1);
	return (0);
}

isc_result_t
isc_time_add(const isc_time_t *t, const interval_t *i, isc_time_t *result)
{
	REQUIRE(t != NULL && i != NULL && result != NULL);
	INSIST(t->nanoseconds < NS_PER_S && i->nanoseconds < NS_PER_S);

	/*
	 * Ensure the resulting seconds value fits in the size of an
	 * unsigned int.  (It is written this way as a slight optimization;
	 * note that even if both values == INT_MAX, then when added
	 * and getting another 1 added below the result is UINT_MAX.)
	 */
	if ((t->seconds > INT_MAX || i->seconds > INT_MAX) &&
	    ((long long)t->seconds + i->seconds > UINT_MAX))
		return (ISC_R_RANGE);

	result->seconds = t->seconds + i->seconds;
	result->nanoseconds = t->nanoseconds + i->nanoseconds;
	if (result->nanoseconds >= NS_PER_S) {
		result->seconds++;
		result->nanoseconds -= NS_PER_S;
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_time_subtract(const isc_time_t *t, const interval_t *i,
		  isc_time_t *result)
{
	REQUIRE(t != NULL && i != NULL && result != NULL);
	INSIST(t->nanoseconds < NS_PER_S && i->nanoseconds < NS_PER_S);

	if ((unsigned int)t->seconds < i->seconds ||
	    ((unsigned int)t->seconds == i->seconds &&
	     t->nanoseconds < i->nanoseconds))
	    return (ISC_R_RANGE);

	result->seconds = t->seconds - i->seconds;
	if (t->nanoseconds >= i->nanoseconds)
		result->nanoseconds = t->nanoseconds - i->nanoseconds;
	else {
		result->nanoseconds = NS_PER_S - i->nanoseconds +
			t->nanoseconds;
		result->seconds--;
	}

	return (ISC_R_SUCCESS);
}

uint64_t
isc_time_microdiff(const isc_time_t *t1, const isc_time_t *t2) {
	uint64_t i1, i2, i3;

	REQUIRE(t1 != NULL && t2 != NULL);
	INSIST(t1->nanoseconds < NS_PER_S && t2->nanoseconds < NS_PER_S);

	i1 = (uint64_t)t1->seconds * NS_PER_S + t1->nanoseconds;
	i2 = (uint64_t)t2->seconds * NS_PER_S + t2->nanoseconds;

	if (i1 <= i2)
		return (0);

	i3 = i1 - i2;

	/*
	 * Convert to microseconds.
	 */
	i3 /= NS_PER_US;

	return (i3);
}

uint32_t
isc_time_seconds(const isc_time_t *t) {
	REQUIRE(t != NULL);
	INSIST(t->nanoseconds < NS_PER_S);

	return ((uint32_t)t->seconds);
}

uint32_t
isc_time_nanoseconds(const isc_time_t *t) {
	REQUIRE(t != NULL);

	ENSURE(t->nanoseconds < NS_PER_S);

	return ((uint32_t)t->nanoseconds);
}

void
isc_time_formattimestamp(const isc_time_t *t, char *buf, unsigned int len) {
	time_t now;
	unsigned int flen;

	REQUIRE(t != NULL);
	INSIST(t->nanoseconds < NS_PER_S);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	now = (time_t) t->seconds;
	flen = strftime(buf, len, "%d-%b-%Y %X", localtime(&now));
	INSIST(flen < len);
	if (flen != 0) {
		snprintf(buf + flen, len - flen,
			 ".%03u", t->nanoseconds / 1000000);
	} else {
		strlcpy(buf, "99-Bad-9999 99:99:99.999", len);
	}
}

void
isc_time_formathttptimestamp(const isc_time_t *t, char *buf, unsigned int len) {
	time_t now;
	unsigned int flen;

	REQUIRE(t != NULL);
	INSIST(t->nanoseconds < NS_PER_S);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	/*
	 * 5 spaces, 1 comma, 3 GMT, 2 %d, 4 %Y, 8 %H:%M:%S, 3+ %a, 3+ %b (29+)
	 */
	now = (time_t)t->seconds;
	flen = strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
	INSIST(flen < len);
}
