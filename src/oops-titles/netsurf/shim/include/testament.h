/*
 * The build's "testament", which `about:testament` and the version banner print.
 *
 * Upstream generates this with `tools/git-testament.pl`, which records the builder's user name,
 * full name and host name, the working tree's absolute path, and every locally modified file.
 * None of that belongs in a published package, so the port supplies it instead: fixed values
 * that describe the source - the NetSurf 3.11 release bundle - and nothing about the machine
 * that compiled it.
 */
#ifndef OOPS_NETSURF_TESTAMENT_H
#define OOPS_NETSURF_TESTAMENT_H

#define USERNAME "oops-apps"
#define GECOS "oops-apps"
#define WT_ROOT "netsurf-all-3.11"
#define WT_HOSTNAME "oops-apps"
#define WT_COMPILEDATE "the NetSurf 3.11 release"
#define WT_BRANCHPATH "release/3.11"
#define WT_BRANCHISTAG 1
#define WT_TAGIS "release/3.11"
#define WT_REVID "netsurf-all-3.11"
#define WT_MODIFIED 0
#define WT_MODIFICATIONS                                                                           \
    {                                                                                              \
    }

#endif /* OOPS_NETSURF_TESTAMENT_H */
