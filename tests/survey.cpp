/*
    Copyright (c) 2012 250bpm s.r.o.
    Copyright (c) 2012 Other contributors as noted in the AUTHORS file

    This file is part of Crossroads I/O project.

    Crossroads I/O is free software; you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Crossroads is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "testutil.hpp"

int XS_TEST_MAIN ()
{
    int rc;
    char buf [32];

    fprintf (stderr, "survey test running...\n");

    //  Create the basic infrastructure.
    void *ctx = xs_init ();
    assert (ctx);
    void *xsurveyor = xs_socket (ctx, XS_XSURVEYOR);
    assert (xsurveyor);
    rc = xs_bind (xsurveyor, "inproc://a");
    assert (rc != -1);
    void *xrespondent = xs_socket (ctx, XS_XRESPONDENT);
    assert (xrespondent);
    rc = xs_bind (xrespondent, "inproc://b");
    assert (rc != -1);
    void *surveyor = xs_socket (ctx, XS_SURVEYOR);
    assert (surveyor);
    rc = xs_connect (surveyor, "inproc://b");
    assert (rc != -1);
    void *respondent1 = xs_socket (ctx, XS_RESPONDENT);
    assert (respondent1);
    rc = xs_connect (respondent1, "inproc://a");
    assert (rc != -1);
    void *respondent2 = xs_socket (ctx, XS_RESPONDENT);
    assert (respondent2);
    rc = xs_connect (respondent2, "inproc://a");
    assert (rc != -1);

    //  Send the survey.
    rc = xs_send (surveyor, "ABC", 3, 0);
    assert (rc == 3);

    //  Forward the survey through the intermediate device.
    //  Survey consist of identity (4 bytes), survey ID (4 bytes) and the body.
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xsurveyor, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xsurveyor, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 3);
    rc = xs_send (xsurveyor, buf, 3, 0);
    assert (rc == 3);

    //  Respondent 1 responds to the survey.
    rc = xs_recv (respondent1, buf, sizeof (buf), 0);
    assert (rc == 3);
    rc = xs_send (respondent1, "DE", 2, 0);
    assert (rc == 2);

    //  Forward the response through the intermediate device.
    rc = xs_recv (xsurveyor, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xsurveyor, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xsurveyor, buf, sizeof (buf), 0);
    assert (rc == 2);
    rc = xs_send (xrespondent, buf, 2, 0);
    assert (rc == 2);

    //  Surveyor gets the response.
    rc = xs_recv (surveyor, buf, sizeof (buf), 0);
    assert (rc == 2);

    //  Respondent 2 responds to the survey.
    rc = xs_recv (respondent2, buf, sizeof (buf), 0);
    assert (rc == 3);
    rc = xs_send (respondent2, "FGHI", 4, 0);
    assert (rc == 4);

    //  Forward the response through the intermediate device.
    rc = xs_recv (xsurveyor, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xsurveyor, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xsurveyor, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, 0);
    assert (rc == 4);

    //  Surveyor gets the response.
    rc = xs_recv (surveyor, buf, sizeof (buf), 0);
    assert (rc == 4);

    //  Now let's test whether survey timeout works as expected.
    int timeout = 100;
    rc = xs_setsockopt (surveyor, XS_SURVEY_TIMEOUT, &timeout, sizeof (int));
    assert (rc == 0);
    rc = xs_send (surveyor, "ABC", 3, 0);
    assert (rc == 3);
    void *watch = xs_stopwatch_start ();
    rc = xs_recv (surveyor, buf, sizeof (buf), 0);
    assert (rc == - 1 && errno == ETIMEDOUT);
    unsigned long elapsed = xs_stopwatch_stop (watch) / 1000;
    time_assert (elapsed, (unsigned long) timeout);

    //  Test whether responses for old surveys are discarded. First,
    //  initiate new survey.
    rc = xs_setsockopt (surveyor, XS_SURVEY_TIMEOUT, &timeout, sizeof (int));
    assert (rc == 0);
    rc = xs_send (surveyor, "DE", 2, 0);
    assert (rc == 2);

    //  Read, process and reply to the old survey.
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 3);
    rc = xs_send (xrespondent, buf, 3, 0);
    assert (rc == 3);

    //  Read, process and reply to the new survey.
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 4);
    rc = xs_send (xrespondent, buf, 4, XS_SNDMORE);
    assert (rc == 4);
    rc = xs_recv (xrespondent, buf, sizeof (buf), 0);
    assert (rc == 2);
    rc = xs_send (xrespondent, buf, 2, 0);
    assert (rc == 2);

    //  Get the response and check it's the response to the new survey and
    //  that response to the old survey was silently discarded.
    rc = xs_recv (surveyor, buf, sizeof (buf), 0);
    assert (rc == 2);

    rc = xs_close (respondent2);
    assert (rc == 0);
    rc = xs_close (respondent1);
    assert (rc == 0);
    rc = xs_close (surveyor);
    assert (rc == 0);
    rc = xs_close (xrespondent);
    assert (rc == 0);
    rc = xs_close (xsurveyor);
    assert (rc == 0);
    rc = xs_term (ctx);
    assert (rc == 0);
    
    return 0 ;
}
