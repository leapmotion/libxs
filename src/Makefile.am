pkgconfigdir = $(libdir)/pkgconfig

if BUILD_LIBZMQ
lib_LTLIBRARIES = libxs.la libzmq.la

pkgconfig_DATA = libxs.pc libzmq.pc

include_HEADERS = ../include/zmq.h ../include/zmq_utils.h
else
lib_LTLIBRARIES = libxs.la

pkgconfig_DATA = libxs.pc
endif

xsincludedir = $(includedir)/xs
xsinclude_HEADERS = ../include/xs/xs.h

libxs_la_SOURCES = \
    address.hpp \
    array.hpp \
    atomic_counter.hpp \
    atomic_ptr.hpp \
    blob.hpp \
    clock.hpp \
    command.hpp \
    config.hpp \
    core.hpp \
    ctx.hpp \
    decoder.hpp \
    devpoll.hpp \
    dist.hpp \
    encoder.hpp \
    epoll.hpp \
    err.hpp \
    fd.hpp \
    fq.hpp \
    io_object.hpp \
    io_thread.hpp \
    ip.hpp \
    ipc_connecter.hpp \
    ipc_listener.hpp \
    i_engine.hpp \
    kqueue.hpp \
    lb.hpp \
    likely.hpp \
    mailbox.hpp \
    msg.hpp \
    mutex.hpp \
    object.hpp \
    options.hpp \
    own.hpp \
    pgm_receiver.hpp \
    pgm_sender.hpp \
    pgm_socket.hpp \
    pipe.hpp \
    platform.hpp \
    poll.hpp \
    polling.hpp \
    pair.hpp \
    prefix_filter.hpp \
    pub.hpp \
    pull.hpp \
    push.hpp \
    random.hpp \
    reaper.hpp \
    rep.hpp \
    req.hpp \
    respondent.hpp \
    select.hpp \
    session_base.hpp \
    signaler.hpp \
    socket_base.hpp \
    stdint.hpp \
    stream_engine.hpp \
    sub.hpp \
    surveyor.hpp \
    tcp_connecter.hpp \
    tcp_listener.hpp \
    thread.hpp \
    topic_filter.hpp \
    upoll.hpp \
    windows.hpp \
    wire.hpp \
    xpub.hpp \
    xrep.hpp \
    xreq.hpp \
    xrespondent.hpp \
    xsub.hpp \
    xsurveyor.hpp \
    ypipe.hpp \
    yqueue.hpp \
    address.cpp \
    clock.cpp \
    core.cpp \
    ctx.cpp \
    decoder.cpp \
    devpoll.cpp \
    dist.cpp \
    encoder.cpp \
    epoll.cpp \
    err.cpp \
    fq.cpp \
    io_object.cpp \
    io_thread.cpp \
    ip.cpp \
    ipc_connecter.cpp \
    ipc_listener.cpp \
    kqueue.cpp \
    lb.cpp \
    mailbox.cpp \
    msg.cpp \
    object.cpp \
    options.cpp \
    own.cpp \
    pair.cpp \
    pgm_receiver.cpp \
    pgm_sender.cpp \
    pgm_socket.cpp \
    pipe.cpp \
    poll.cpp \
    prefix_filter.cpp \
    pull.cpp \
    push.cpp \
    reaper.cpp \
    pub.cpp \
    random.cpp \
    rep.cpp \
    req.cpp \
    respondent.cpp \
    select.cpp \
    session_base.cpp \
    signaler.cpp \
    socket_base.cpp \
    stream_engine.cpp \
    sub.cpp \
    surveyor.cpp \
    tcp_connecter.cpp \
    tcp_listener.cpp \
    thread.cpp \
    topic_filter.cpp \
    upoll.cpp \
    xpub.cpp \
    xrep.cpp \
    xreq.cpp \
    xrespondent.cpp \
    xsub.cpp \
    xsurveyor.cpp \
    xs.cpp

if ON_MINGW
libxs_la_LDFLAGS = -no-undefined -avoid-version -version-info @LIBXS_ABI_VERSION@ @LIBXS_EXTRA_LDFLAGS@
else
libxs_la_LDFLAGS = -version-info @LIBXS_ABI_VERSION@ @LIBXS_EXTRA_LDFLAGS@
endif

libxs_la_CPPFLAGS = -DXS_LIBDIR_PATH='"$(libdir)"'
libxs_la_CXXFLAGS = @LIBXS_EXTRA_CXXFLAGS@

if BUILD_PGM
libxs_la_CPPFLAGS += -I$(top_srcdir)/@pgm_srcdir@/include/
libxs_la_LIBADD = $(top_srcdir)/@pgm_srcdir@/libpgm_noinst.la
endif

if BUILD_LIBZMQ
libzmq_la_SOURCES = xszmq.cpp
libzmq_la_CPPFLAGS = -DXS_BUILDING_LIBXSZMQ
libzmq_la_LIBADD = libxs.la

if ON_MINGW
libzmq_la_LDFLAGS = -no-undefined -avoid-version -version-info @LIBZMQ_ABI_VERSION@
else
libzmq_la_LDFLAGS = -version-info @LIBZMQ_ABI_VERSION@
endif
endif

dist-hook:
		-rm $(distdir)/platform.hpp

