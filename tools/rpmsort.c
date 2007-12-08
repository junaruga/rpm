#include "system.h"
const char *__progname;

#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmurl.h>

#include <rpm/rpmdb.h>
#include <rpm/rpmps.h>
#include <rpm/rpmte.h>
#include <rpm/rpmts.h>
#include <rpm/rpmds.h>

#include "lib/manifest.h"
#include <rpm/rpmlog.h>
#include "debug.h"

static int _depends_debug;

static int noAvailable = 1;
#ifdef DYING
static const char * avdbpath =
	"/usr/lib/rpmdb/%{_arch}-%{_vendor}-%{_os}/redhat";
#endif
static int noDeps = 0;

static inline const char * identifyDepend(int32_t f)
{
    if (isLegacyPreReq(f))
	return "PreReq:";
    f = _notpre(f);
    if (f & RPMSENSE_SCRIPT_PRE)
	return "Requires(pre):";
    if (f & RPMSENSE_SCRIPT_POST)
	return "Requires(post):";
    if (f & RPMSENSE_SCRIPT_PREUN)
	return "Requires(preun):";
    if (f & RPMSENSE_SCRIPT_POSTUN)
	return "Requires(postun):";
    if (f & RPMSENSE_SCRIPT_VERIFY)
	return "Requires(verify):";
    if (f & RPMSENSE_FIND_REQUIRES)
	return "Requires(auto):";
    return "Requires:";
}

static int
do_tsort(const char *fileArgv[])
{
    rpmts ts = NULL;
    const char ** pkgURL = NULL;
    char * pkgState = NULL;
    const char ** fnp;
    const char * fileURL = NULL;
    int numPkgs = 0;
    int numFailed = 0;
    int prevx;
    int pkgx;
    const char ** argv = NULL;
    int argc = 0;
    const char ** av = NULL;
    int ac = 0;
    Header h;
    int rc = 0;
    int i;

    if (fileArgv == NULL)
	return 0;

    ts = rpmtsCreate();

    rc = rpmtsOpenDB(ts, O_RDONLY);
    if (rc) {
	rpmlog(RPMLOG_ERR, _("cannot open Packages database\n"));
	rc = -1;
	goto exit;
    }

#ifdef	DYING
    /* Load all the available packages. */
    if (!(noDeps || noAvailable)) {
	rpmdbMatchIterator mi = NULL;
	struct rpmdb_s * avdb = NULL;
	const char * rootdir = "/";

	addMacro(NULL, "_dbpath", NULL, avdbpath, RMIL_CMDLINE);
	rc = rpmdbOpen(rootdir, &avdb, O_RDONLY, 0644);
	delMacro(NULL, "_dbpath");
	if (rc) {
	    rpmlog(RPMLOG_ERR, _("cannot open Available database\n"));
	    goto endavail;
	}
        mi = rpmdbInitIterator(avdb, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    rpmtsAvailablePackage(ts, h, NULL);
	}

endavail:
	if (mi) rpmdbFreeIterator(mi);
	if (avdb) rpmdbClose(avdb);
    }
#endif

    /* Build fully globbed list of arguments in argv[argc]. */
    for (fnp = fileArgv; *fnp; fnp++) {
	av = _free(av);
	ac = 0;
	rc = rpmGlob(*fnp, &ac, &av);
	if (rc || ac == 0) continue;

	argv = (argc == 0)
	    ? xmalloc((argc+2) * sizeof(*argv))
	    : xrealloc(argv, (argc+2) * sizeof(*argv));
	memcpy(argv+argc, av, ac * sizeof(*av));
	argc += ac;
	argv[argc] = NULL;
    }
    av = _free(av);

    numPkgs = 0;
    prevx = 0;
    pkgx = 0;

restart:
    /* Allocate sufficient storage for next set of args. */
    if (pkgx >= numPkgs) {
	numPkgs = pkgx + argc;
	pkgURL = (pkgURL == NULL)
	    ? xmalloc( (numPkgs + 1) * sizeof(*pkgURL))
	    : xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
	memset(pkgURL + pkgx, 0, ((argc + 1) * sizeof(*pkgURL)));
	pkgState = (pkgState == NULL)
	    ? xmalloc( (numPkgs + 1) * sizeof(*pkgState))
	    : xrealloc(pkgState, (numPkgs + 1) * sizeof(*pkgState));
	memset(pkgState + pkgx, 0, ((argc + 1) * sizeof(*pkgState)));
    }

    /* Copy next set of args. */
    for (i = 0; i < argc; i++) {
	fileURL = _free(fileURL);
	fileURL = argv[i];
	argv[i] = NULL;
	pkgURL[pkgx] = fileURL;
	fileURL = NULL;
	pkgx++;
    }
    fileURL = _free(fileURL);

    /* Continue processing file arguments, building transaction set. */
    for (fnp = pkgURL+prevx; *fnp; fnp++, prevx++) {
	const char * fileName;
	FD_t fd;

	(void) urlPath(*fnp, &fileName);

	/* Try to read the header from a package file. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    continue;
	}

	rc = rpmReadPackageFile(ts, fd, *fnp, &h);
	Fclose(fd);

	if (rc == RPMRC_OK) {
	    rc = rpmtsAddInstallElement(ts, h, (fnpyKey)fileName, 0, NULL);
	    h = headerFree(h); 
	    continue;
	}

	if (rc != RPMRC_NOTFOUND) {
	    rpmlog(RPMLOG_ERR, _("%s cannot be installed\n"), *fnp);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Try to read a package manifest. */
	fd = Fopen(*fnp, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc)
	    rpmlog(RPMLOG_NOTICE, _("%s: read manifest failed: %s\n"),
			fileURL, Fstrerror(fd));
	Fclose(fd);

	/* If successful, restart the query loop. */
	if (rc == 0) {
	    prevx++;
	    goto restart;
	}

	numFailed++; *fnp = NULL;
	break;
    }

    if (numFailed) goto exit;

    if (!noDeps) {
	rpmps ps;

	rc = rpmtsCheck(ts);

	ps = rpmtsProblems(ts);
	if (ps) {
	    rpmlog(RPMLOG_ERR, _("Failed dependencies:\n"));
	    rpmpsPrint(NULL, ps);
	    ps = rpmpsFree(ps);
	    rc = -1;
	    goto exit;
	}
	ps = rpmpsFree(ps);
    }

    rc = rpmtsOrder(ts);
    if (rc)
	goto exit;

    {	rpmtsi pi;
	rpmte p;
	rpmte q;
	int oType = TR_ADDED;

fprintf(stdout, "digraph XXX {\n");

fprintf(stdout, "  rankdir=LR\n");

fprintf(stdout, "//===== Packages:\n");
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, oType)) != NULL) {
fprintf(stdout, "//%5d%5d %s\n", rpmteTree(p), rpmteDepth(p), rpmteN(p));
	    q = rpmteParent(p);
	    if (q != NULL)
fprintf(stdout, "  \"%s\" -> \"%s\"\n", rpmteN(p), rpmteN(q));
	    else {
fprintf(stdout, "  \"%s\"\n", rpmteN(p));
fprintf(stdout, "  { rank=max ; \"%s\" }\n", rpmteN(p));
	    }
	}
	pi = rpmtsiFree(pi);

fprintf(stdout, "}\n");

    }

    rc = 0;

exit:
    for (i = 0; i < numPkgs; i++) {
        pkgURL[i] = _free(pkgURL[i]);
    }
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);

    ts = rpmtsFree(ts);
    return rc;
}

static struct poptOption optionsTable[] = {
 { "noavailable", '\0', 0, &noAvailable, 0,	NULL, NULL},
 { "nodeps", '\0', 0, &noDeps, 0,		NULL, NULL},
 { "verbose", 'v', 0, 0, 'v',			NULL, NULL},
 { NULL,	0, 0, 0, 0,			NULL, NULL}
};

int
main(int argc, char *argv[])
{
    poptContext optCon;
    const char * optArg;
    int arg;
    int ec = 0;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */
#if defined(ENABLE_NLS)
    (void)setlocale(LC_ALL, "" );

    (void)bindtextdomain(PACKAGE, LOCALEDIR);
    (void)textdomain(PACKAGE);
#endif

    _depends_debug = 1;

    optCon = poptGetContext("rpmsort", argc, (const char **) argv, optionsTable, 0);
#if RPM_USES_POPTREADDEFAULTCONFIG
    poptReadDefaultConfig(optCon, 1);
#endif

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (arg) {
	case 'v':
	    rpmIncreaseVerbosity();
	    break;
	default:
	    fprintf(stderr, _("unknown popt return (%d)"), arg);
	    exit(EXIT_FAILURE);
	    break;
	}
    }

    rpmReadConfigFiles(NULL, NULL);

    ec = do_tsort(poptGetArgs(optCon));

    optCon = poptFreeContext(optCon);

    return ec;
}
