#ifndef	_RPMTE_INTERNAL_H
#define _RPMTE_INTERNAL_H

#include <rpm/rpmte.h>

/** \ingroup rpmte
 * Dependncy ordering information.
 */
struct tsortInfo_s {
    union {
	int	count;
	rpmte	suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
    struct tsortInfo_s * tsi_next;
    rpmte tsi_chain;
    int		tsi_reqx;
    int		tsi_qcnt;
};

/** \ingroup rpmte
 * A single package instance to be installed/removed atomically.
 */
struct rpmte_s {
    rpmElementType type;	/*!< Package disposition (installed/removed). */

    Header h;			/*!< Package header. */
    const char * NEVR;		/*!< Package name-version-release. */
    const char * NEVRA;		/*!< Package name-version-release.arch. */
    const char * name;		/*!< Name: */
    char * epoch;
    char * version;		/*!< Version: */
    char * release;		/*!< Release: */
    const char * arch;		/*!< Architecture hint. */
    const char * os;		/*!< Operating system hint. */
    int archScore;		/*!< (TR_ADDED) Arch score. */
    int osScore;		/*!< (TR_ADDED) Os score. */
    int isSource;		/*!< (TR_ADDED) source rpm? */

    rpmte parent;		/*!< Parent transaction element. */
    int degree;			/*!< No. of immediate children. */
    int npreds;			/*!< No. of predecessors. */
    int tree;			/*!< Tree index. */
    int depth;			/*!< Depth in dependency tree. */
    int breadth;		/*!< Breadth in dependency tree. */
    unsigned int db_instance;   /*!< Database Instance after add */
    tsortInfo tsi;		/*!< Dependency ordering chains. */

    rpmds this;			/*!< This package's provided NEVR. */
    rpmds provides;		/*!< Provides: dependencies. */
    rpmds requires;		/*!< Requires: dependencies. */
    rpmds conflicts;		/*!< Conflicts: dependencies. */
    rpmds obsoletes;		/*!< Obsoletes: dependencies. */
    rpmfi fi;			/*!< File information. */

    uint32_t color;		/*!< Color bit(s) from package dependencies. */
    uint32_t pkgFileSize;	/*!< No. of bytes in package file (approx). */

    fnpyKey key;		/*!< (TR_ADDED) Retrieval key. */
    rpmRelocation * relocs;	/*!< (TR_ADDED) Payload file relocations. */
    int nrelocs;		/*!< (TR_ADDED) No. of relocations. */
    int autorelocatex;		/*!< (TR_ADDED) Auto relocation entry index. */
    FD_t fd;			/*!< (TR_ADDED) Payload file descriptor. */

    union {
	rpmalKey addedKey;
	struct {
	    rpmalKey dependsOnKey;
	    int dboffset;
	} removed;
    } u;

};

/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct rpmtsi_s {
    rpmts ts;		/*!< transaction set. */
    int reverse;	/*!< reversed traversal? */
    int ocsave;		/*!< last returned iterator index. */
    int oc;		/*!< iterator index. */
};

#endif	/* _RPMTE_INTERNAL_H */

