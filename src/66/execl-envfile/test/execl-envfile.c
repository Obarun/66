#include <assert.h>
#include <stdio.h>

#include <oblibs/sbl.h>
#include <oblibs/subst.h>
#include <oblibs/environ.h>

void envfile(const char *efile, const char *script, const char **env, const char *expect)
{
    _cleanup_strbuf_ strbuf saenv = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf cmdline = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;

    assert(environ_merge_string(&saenv, efile) == 1) ;
    assert(environ_substitute(&saenv, &info) == 1) ;
    assert(environ_clean_unexport(&saenv) == 1) ;

    size_t elen = environ_length(env) ;
    size_t n = elen + 1 +  sbl_count(&saenv) ;

    char const *nenv[n + 1] ;
    assert(environ_merge(nenv, n , env, elen, saenv.s, saenv.len) != 0) ;

    int r = subst(&cmdline, script, strlen(script), &info) ;

    assert(r >= 0) ;
    if (!r)
      return ;

    assert(sbl_rebuild_oneline(&cmdline) == 1) ;
    assert(strcmp(cmdline.s, expect) == 0) ;
    subst_free(&info) ;
}

void envfile_fail(const char *efile, const char *script, const char **env, const char *expect)
{
    _cleanup_strbuf_ strbuf saenv = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf cmdline = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;
    assert(environ_merge_string(&saenv, efile) == 1) ;
    assert(environ_substitute(&saenv, &info) == 1) ;
    assert(environ_clean_unexport(&saenv) == 1) ;

    size_t elen = environ_length(env) ;
    size_t n = elen + 1 +  sbl_count(&saenv) ;

    char const *nenv[n + 1] ;
    assert(environ_merge(nenv, n , env, elen, saenv.s, saenv.len) != 0) ;

    int r = subst(&cmdline, script, strlen(script), &info) ;

    assert(r >= 0) ;
    if (!r)
      return ;

    assert(sbl_rebuild_oneline(&cmdline) == 1) ;
    assert(strcmp(cmdline.s, expect) != 0) ;
    subst_free(&info) ;
}

void basic(void)
{
    printf("Running test basic...\n") ;

    const char *efile = "Args=-d\n\
SocketDir=/run/openntpd\n\
Socket=!${SocketDir}/openntpd.sock\n\
ImportFile=!/etc/ly/config.ini" ;

    char const *env[2] = { "Args=-c", 0} ;

    char const *script = "${Args} ${SocketDir} ${Socket} ${ImportFile}" ;

    const char *expect = "-d /run/openntpd /run/openntpd/openntpd.sock /etc/ly/config.ini" ;

    envfile(efile, script, env, expect) ;

    return ;
}

void complex(void)
{
    printf("Running test complex...\n") ;

    const char *efile = "Args=!-f -n ${ArgsConfDirectory} ${ArgsSocket}\n\
ArgsConfDirectory=!-c /etc/acpi/events\n\
ArgsSocket=!-s ${Socket}\n\
Socket=/run/acpid.socket" ;

    const char *env[2] = { "Args=-c", 0} ;

    const char *script = "${Args} ${ArgsConfDirectory} ${ArgsSocket} ${Socket}" ;

    const char *expect = "-f -n -c /etc/acpi/events -s /run/acpid.socket -c /etc/acpi/events -s /run/acpid.socket /run/acpid.socket" ;

    envfile(efile, script, env, expect) ;

    return ;
}

void complex1(void)
{
    printf("Running test complex1...\n") ;

    const char *efile = "ArgsStart=!-d -v ${ArgsConfFile} ${ArgsPidFile}\n\
ArgsStop=!-r\n\
ArgsConfFile=!-cf ${ConfDirectory}/${ConfFile}\n\
ArgsPidFile=!-pf ${PidFile}\n\
ConfDirectory=!/etc/dhcp\n\
ConfFile=!dhclient-@I.conf\n\
LiveDirectory=!/run/dhclient-@I\n\
PidFile=${LiveDirectory}/dhclient.pid" ;

    const char *env[2] = { "MyArgs=-c", 0} ;

    const char *script = "${ArgsStart} \
${ArgsStop} \
${ArgsConfFile} \
${ArgsPidFile} \
${ConfDirectory} \
${ConfFile} \
${LiveDirectory} \
${PidFile} \
${MyArgs}" ;

    const char *expect = "-d -v -cf /etc/dhcp/dhclient-@I.conf -pf /run/dhclient-@I/dhclient.pid \
-r \
-cf /etc/dhcp/dhclient-@I.conf \
-pf /run/dhclient-@I/dhclient.pid \
/etc/dhcp \
dhclient-@I.conf \
/run/dhclient-@I \
/run/dhclient-@I/dhclient.pid \
${MyArgs}" ;

    envfile(efile, script, env, expect) ;

    return ;
}

void complex2(void)
{
    printf("Running test complex2...\n") ;

    const char *efile = "Args=!${ArgsConfFile} ${ArgsPidFile}\n\
ArgsConfFile=!−−configfile=/etc/metalog/metalog.conf\n\
ArgsPidFile=!−−pidfile=${PidFile}\n\
PidFile=!/run/metalog.pid" ;

    const char *env[2] = { "MyArgs=-c", 0} ;

    const char *script = "${Args} \
${ArgsConfFile} \
${ArgsPidFile} \
${MyArgs}" ;

    const char *expect = "−−configfile=/etc/metalog/metalog.conf −−pidfile=/run/metalog.pid \
−−configfile=/etc/metalog/metalog.conf \
−−pidfile=/run/metalog.pid \
${MyArgs}" ;

    envfile(efile, script, env, expect) ;

    return ;
}

void comment(void)
{
    printf("Running test comment...\n") ;

    const char *efile = "ConfFile=!%%initconf%%\n\
## Time to wait to bring up and down services in milliseconds\n\
Timeout=!60000\n\
Verbosity=3" ;

    const char *env[2] = { "Args=-c", 0} ;

    const char *script = "${ConfFile} ${Timeout} ${Verbosity}" ;

    const char *expect = "%%initconf%% 60000 3" ;

    envfile(efile, script, env, expect) ;

    return ;
}

void comment1(void)
{
    printf("Running test comment1...\n") ;

    const char *efile = "## Uncomment it to use a display manager.\n\
## Can be any display manager as long as the\n\
## corresponding frontend file exist on your system\n\
## e.g sddm,lightdm,...\n\
## It also prepare the .xsession file.\n\
\n\
displaymanager = !lxdm\n\
\n\
## Uncomment it to use a console tracker.\n\
## Can be any console tracker as long as the\n\
## corresponding frontend file exist on your system\n\
## e.g consolekit,seatd,turnstile,...\n\
\n\
consoletracker = consolekit" ;

    const char *env[2] = { "Args=-c", 0} ;

    const char *script = "${Args} ${displaymanager} ${consoletracker}" ;

    const char *expect = "${Args} lxdm consolekit" ;

    envfile(efile, script, env, expect) ;

    return ;
}

void fail(void)
{
    printf("Running test fail...\n") ;

    const char *efile = "## Uncomment it to use a display manager.\n\
## Can be any display manager as long as the\n\
## corresponding frontend file exist on your system\n\
## e.g sddm,lightdm,...\n\
## It also prepare the .xsession file.\n\
\n\
displaymanager = !lxdm\n\
\n\
## Uncomment it to use a console tracker.\n\
## Can be any console tracker as long as the\n\
## corresponding frontend file exist on your system\n\
## e.g consolekit,seatd,turnstile,...\n\
\n\
consoletracker = consolekit" ;

    const char *env[2] = { "Args=-c", 0} ;

    const char *script = "${Args} ${displaymanager} ${consoletracker}" ;

    const char *expect = "-c lxdm consolekit" ;

    envfile_fail(efile, script, env, expect) ;

    return ;
}

void recursive(void)
{
    printf("Running test recursive...\n") ;

    const char *efile = "A=${B}\n\
B=${C}\n\
C=${D}/3\n\
D=4 * 3" ;

    const char *env[2] = { "Args=-c", 0} ;

    const char *script = "${A} ${B} ${C}" ;

    const char *expect = "4 * 3/3 4 * 3/3 4 * 3/3" ;

    envfile(efile, script, env, expect) ;

    return ;
}

int main(void)
{
    basic() ;
    complex() ;
    complex1() ;
    complex2() ;
    comment() ;
    comment1() ;
    fail() ;
     recursive() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}