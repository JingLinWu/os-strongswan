/**
 * @file x509.c
 * 
 * @brief Implementation of x509_t.
 * 
 */

/*
 * Copyright (C) 2000 Andreas Hess, Patric Lichtsteiner, Roger Wegmann
 * Copyright (C) 2001 Marco Bertossa, Andreas Schleiss
 * Copyright (C) 2002 Mario Strasser
 * Copyright (C) 2000-2004 Andreas Steffen, Zuercher Hochschule Winterthur
 * Copyright (C) 2006 Martin Willi, Andreas Steffen
 *
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * RCSID $Id: x509.c 3355 2007-11-20 12:06:40Z martin $
 */

#include <gmp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "x509.h"
#include "hashers/hasher.h"
#include <library.h>
#include <debug.h>
#include <asn1/oid.h>
#include <asn1/asn1.h>
#include <asn1/pem.h>
#include <utils/linked_list.h>
#include <utils/identification.h>

#define CERT_WARNING_INTERVAL	30	/* days */

/**
 * Different kinds of generalNames
 */
typedef enum generalNames_t generalNames_t;

enum generalNames_t {
	GN_OTHER_NAME =		0,
	GN_RFC822_NAME =	1,
	GN_DNS_NAME =		2,
	GN_X400_ADDRESS =	3,
	GN_DIRECTORY_NAME =	4,
	GN_EDI_PARTY_NAME = 5,
	GN_URI =			6,
	GN_IP_ADDRESS =		7,
	GN_REGISTERED_ID =	8,
};

typedef struct private_x509_t private_x509_t;

/**
 * Private data of a x509_t object.
 */
struct private_x509_t {
	/**
	 * Public interface for this certificate.
	 */
	x509_t public;
	
	/**
	 * Time when certificate was installed
	 */
	time_t installed;

	/**
	 * Time until certificate can be trusted
	 */
	time_t until;

	/**
	 * Certificate status
	 */
	cert_status_t status;

	/**
	 * Authority flags
	 */
	 u_int authority_flags;

	/**
	 * X.509 Certificate in DER format
	 */
	chunk_t certificate;

	/**
	 * X.509 certificate body over which signature is computed
	 */
	chunk_t tbsCertificate;

	/**
	 * Version of the X.509 certificate
	 */
	u_int version;
	
	/**
	 * Serial number of the X.509 certificate
	 */
	chunk_t serialNumber;

	/**
	 * Signature algorithm
	 */
	int signatureAlgorithm;
	
	/**
	 * ID representing the certificate issuer
	 */
	identification_t *issuer;
	
	/**
	 * link to the info recored of the certificate issuer
	 */
	ca_info_t *ca_info;

	/**
	 * Start time of certificate validity
	 */
	time_t notBefore;

	/**
	 * End time of certificate validity
	 */
	time_t notAfter;

	/**
	 * ID representing the certificate subject
	 */
	identification_t *subject;
	
	/**
	 * List of identification_t's representing subjectAltNames
	 */
	linked_list_t *subjectAltNames;
	
	/**
	 * List of identification_t's representing crlDistributionPoints
	 */
	linked_list_t *crlDistributionPoints;

	/**
	 * List of identification_t's representing ocspAccessLocations
	 */
	linked_list_t *ocspAccessLocations;

	/**
	 * Subject public key
	 */
	chunk_t subjectPublicKey;

	/**
	 * Subject RSA public key, if subjectPublicKeyAlgorithm == RSA
	 */
	rsa_public_key_t *public_key;
	
	/**
	 * Subject Key Identifier
	 */
	chunk_t subjectKeyID;

	/**
	 * Authority Key Identifier
	 */
	chunk_t authKeyID;

	/**
	 * Authority Key Serial Number
	 */
	chunk_t authKeySerialNumber;
	
	/**
	 * Indicates if the certificate is self-signed
	 */
	bool isSelfSigned;

	/**
	 * CA basic constraints flag
	 */
	bool isCA;

	/**
	 * OCSPSigner extended key usage flag
	 */
	bool isOcspSigner;

	/**
	 * Signature
	 */
	chunk_t signature;

};

/**
 * ASN.1 definition of generalName 
 */
static const asn1Object_t generalNameObjects[] = {
	{ 0,   "otherName",		ASN1_CONTEXT_C_0,  ASN1_OPT|ASN1_BODY	}, /*  0 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /*  1 */
	{ 0,   "rfc822Name",	ASN1_CONTEXT_S_1,  ASN1_OPT|ASN1_BODY	}, /*  2 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END 			}, /*  3 */
	{ 0,   "dnsName",		ASN1_CONTEXT_S_2,  ASN1_OPT|ASN1_BODY	}, /*  4 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /*  5 */
	{ 0,   "x400Address",	ASN1_CONTEXT_S_3,  ASN1_OPT|ASN1_BODY	}, /*  6 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /*  7 */
	{ 0,   "directoryName",	ASN1_CONTEXT_C_4,  ASN1_OPT|ASN1_BODY	}, /*  8 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /*  9 */
	{ 0,   "ediPartyName",	ASN1_CONTEXT_C_5,  ASN1_OPT|ASN1_BODY	}, /* 10 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /* 11 */
	{ 0,   "URI",			ASN1_CONTEXT_S_6,  ASN1_OPT|ASN1_BODY	}, /* 12 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /* 13 */
	{ 0,   "ipAddress",		ASN1_CONTEXT_S_7,  ASN1_OPT|ASN1_BODY	}, /* 14 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}, /* 15 */
	{ 0,   "registeredID",	ASN1_CONTEXT_S_8,  ASN1_OPT|ASN1_BODY	}, /* 16 */
	{ 0,   "end choice",	ASN1_EOC,          ASN1_END				}  /* 17 */
};
#define GN_OBJ_OTHER_NAME	 	 0
#define GN_OBJ_RFC822_NAME	 	 2
#define GN_OBJ_DNS_NAME		 	 4
#define GN_OBJ_X400_ADDRESS	 	 6
#define GN_OBJ_DIRECTORY_NAME	 8
#define GN_OBJ_EDI_PARTY_NAME	10
#define GN_OBJ_URI				12
#define GN_OBJ_IP_ADDRESS		14
#define GN_OBJ_REGISTERED_ID	16
#define GN_OBJ_ROOF				18

/**
 * ASN.1 definition of otherName 
 */
static const asn1Object_t otherNameObjects[] = {
	{0, "type-id",	ASN1_OID,			ASN1_BODY	}, /*  0 */
	{0, "value",	ASN1_CONTEXT_C_0,	ASN1_BODY	}  /*  1 */
};
#define ON_OBJ_ID_TYPE		0
#define ON_OBJ_VALUE		1
#define ON_OBJ_ROOF			2
/**
 * ASN.1 definition of a basicConstraints extension 
 */
static const asn1Object_t basicConstraintsObjects[] = {
	{ 0, "basicConstraints",	ASN1_SEQUENCE,	ASN1_NONE			}, /*  0 */
	{ 1,   "CA",				ASN1_BOOLEAN,	ASN1_DEF|ASN1_BODY	}, /*  1 */
	{ 1,   "pathLenConstraint",	ASN1_INTEGER,	ASN1_OPT|ASN1_BODY	}, /*  2 */
	{ 1,   "end opt",			ASN1_EOC,		ASN1_END  			}  /*  3 */
};
#define BASIC_CONSTRAINTS_CA	1
#define BASIC_CONSTRAINTS_ROOF	4

/** 
 * ASN.1 definition of a keyIdentifier 
 */
static const asn1Object_t keyIdentifierObjects[] = {
	{ 0,   "keyIdentifier",	ASN1_OCTET_STRING,	ASN1_BODY }  /*  0 */
};

/**
 * ASN.1 definition of a authorityKeyIdentifier extension 
 */
static const asn1Object_t authorityKeyIdentifierObjects[] = {
	{ 0,   "authorityKeyIdentifier",	ASN1_SEQUENCE,		ASN1_NONE 			}, /*  0 */
	{ 1,     "keyIdentifier",			ASN1_CONTEXT_S_0,	ASN1_OPT|ASN1_OBJ	}, /*  1 */
	{ 1,     "end opt",					ASN1_EOC,			ASN1_END  			}, /*  2 */
	{ 1,     "authorityCertIssuer",		ASN1_CONTEXT_C_1,	ASN1_OPT|ASN1_OBJ	}, /*  3 */
	{ 1,     "end opt",					ASN1_EOC,			ASN1_END  			}, /*  4 */
	{ 1,     "authorityCertSerialNumber",ASN1_CONTEXT_S_2,  ASN1_OPT|ASN1_BODY	}, /*  5 */
	{ 1,     "end opt",					ASN1_EOC,			ASN1_END  			}  /*  6 */
};
#define AUTH_KEY_ID_KEY_ID			1
#define AUTH_KEY_ID_CERT_ISSUER		3
#define AUTH_KEY_ID_CERT_SERIAL		5
#define AUTH_KEY_ID_ROOF			7

/**
 * ASN.1 definition of a authorityInfoAccess extension 
 */
static const asn1Object_t authorityInfoAccessObjects[] = {
	{ 0,   "authorityInfoAccess",	ASN1_SEQUENCE,	ASN1_LOOP }, /*  0 */
	{ 1,     "accessDescription",	ASN1_SEQUENCE,	ASN1_NONE }, /*  1 */
	{ 2,       "accessMethod",		ASN1_OID,		ASN1_BODY }, /*  2 */
	{ 2,       "accessLocation",	ASN1_EOC,		ASN1_RAW  }, /*  3 */
	{ 0,   "end loop",				ASN1_EOC,		ASN1_END  }  /*  4 */
};
#define AUTH_INFO_ACCESS_METHOD		2
#define AUTH_INFO_ACCESS_LOCATION	3
#define AUTH_INFO_ACCESS_ROOF		5

/**
 * ASN.1 definition of a extendedKeyUsage extension
 */
static const asn1Object_t extendedKeyUsageObjects[] = {
	{ 0, "extendedKeyUsage",	ASN1_SEQUENCE,	ASN1_LOOP }, /*  0 */
	{ 1,   "keyPurposeID",		ASN1_OID,		ASN1_BODY }, /*  1 */
	{ 0, "end loop",			ASN1_EOC,		ASN1_END  }, /*  2 */
};

#define EXT_KEY_USAGE_PURPOSE_ID	1
#define EXT_KEY_USAGE_ROOF			3

/**
 * ASN.1 definition of generalNames 
 */
static const asn1Object_t generalNamesObjects[] = {
	{ 0, "generalNames",	ASN1_SEQUENCE,	ASN1_LOOP }, /*  0 */
	{ 1,   "generalName",	ASN1_EOC,		ASN1_RAW  }, /*  1 */
	{ 0, "end loop",		ASN1_EOC,		ASN1_END  }  /*  2 */
};
#define GENERAL_NAMES_GN	1
#define GENERAL_NAMES_ROOF	3


/**
 * ASN.1 definition of crlDistributionPoints
 */
static const asn1Object_t crlDistributionPointsObjects[] = {
	{ 0, "crlDistributionPoints",	ASN1_SEQUENCE,		ASN1_LOOP			}, /*  0 */
	{ 1,   "DistributionPoint",		ASN1_SEQUENCE,		ASN1_NONE			}, /*  1 */
	{ 2,     "distributionPoint",	ASN1_CONTEXT_C_0,	ASN1_OPT|ASN1_LOOP	}, /*  2 */
	{ 3,       "fullName",			ASN1_CONTEXT_C_0,	ASN1_OPT|ASN1_OBJ	}, /*  3 */
	{ 3,       "end choice",		ASN1_EOC,			ASN1_END			}, /*  4 */
	{ 3,       "nameRelToCRLIssuer",ASN1_CONTEXT_C_1,	ASN1_OPT|ASN1_BODY	}, /*  5 */
	{ 3,       "end choice",		ASN1_EOC,			ASN1_END			}, /*  6 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /*  7 */
	{ 2,     "reasons",				ASN1_CONTEXT_C_1,	ASN1_OPT|ASN1_BODY	}, /*  8 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /*  9 */
	{ 2,     "crlIssuer",			ASN1_CONTEXT_C_2,	ASN1_OPT|ASN1_BODY	}, /* 10 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 11 */
	{ 0, "end loop",				ASN1_EOC,			ASN1_END			}, /* 12 */
};
#define CRL_DIST_POINTS_FULLNAME	 3
#define CRL_DIST_POINTS_ROOF		13

/**
 * ASN.1 definition of an X.509v3 x509
 */
static const asn1Object_t certObjects[] = {
	{ 0, "x509",					ASN1_SEQUENCE,		ASN1_OBJ			}, /*  0 */
	{ 1,   "tbsCertificate",		ASN1_SEQUENCE,		ASN1_OBJ			}, /*  1 */
	{ 2,     "DEFAULT v1",			ASN1_CONTEXT_C_0,	ASN1_DEF			}, /*  2 */
	{ 3,       "version",			ASN1_INTEGER,		ASN1_BODY			}, /*  3 */
	{ 2,     "serialNumber",		ASN1_INTEGER,		ASN1_BODY			}, /*  4 */
	{ 2,     "signature",			ASN1_EOC,			ASN1_RAW			}, /*  5 */
	{ 2,     "issuer",				ASN1_SEQUENCE,		ASN1_OBJ			}, /*  6 */
	{ 2,     "validity",			ASN1_SEQUENCE,		ASN1_NONE			}, /*  7 */
	{ 3,       "notBefore",			ASN1_EOC,			ASN1_RAW			}, /*  8 */
	{ 3,       "notAfter",			ASN1_EOC,			ASN1_RAW			}, /*  9 */
	{ 2,     "subject",				ASN1_SEQUENCE,		ASN1_OBJ			}, /* 10 */
	{ 2,     "subjectPublicKeyInfo",ASN1_SEQUENCE,		ASN1_NONE			}, /* 11 */
	{ 3,       "algorithm",			ASN1_EOC,			ASN1_RAW			}, /* 12 */
	{ 3,       "subjectPublicKey",	ASN1_BIT_STRING,	ASN1_NONE			}, /* 13 */
	{ 4,         "RSAPublicKey",	ASN1_SEQUENCE,		ASN1_RAW			}, /* 14 */
	{ 2,     "issuerUniqueID",		ASN1_CONTEXT_C_1,	ASN1_OPT			}, /* 15 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 16 */
	{ 2,     "subjectUniqueID",		ASN1_CONTEXT_C_2,	ASN1_OPT			}, /* 17 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 18 */
	{ 2,     "optional extensions",	ASN1_CONTEXT_C_3,	ASN1_OPT			}, /* 19 */
	{ 3,       "extensions",		ASN1_SEQUENCE,		ASN1_LOOP			}, /* 20 */
	{ 4,         "extension",		ASN1_SEQUENCE,		ASN1_NONE			}, /* 21 */
	{ 5,           "extnID",		ASN1_OID,			ASN1_BODY			}, /* 22 */
	{ 5,           "critical",		ASN1_BOOLEAN,		ASN1_DEF|ASN1_BODY	}, /* 23 */
	{ 5,           "extnValue",		ASN1_OCTET_STRING,	ASN1_BODY			}, /* 24 */
	{ 3,       "end loop",			ASN1_EOC,			ASN1_END			}, /* 25 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 26 */
	{ 1,   "signatureAlgorithm",	ASN1_EOC,			ASN1_RAW			}, /* 27 */
	{ 1,   "signatureValue",		ASN1_BIT_STRING,	ASN1_BODY			}  /* 28 */
};
#define X509_OBJ_CERTIFICATE					 0
#define X509_OBJ_TBS_CERTIFICATE				 1
#define X509_OBJ_VERSION						 3
#define X509_OBJ_SERIAL_NUMBER					 4
#define X509_OBJ_SIG_ALG						 5
#define X509_OBJ_ISSUER 						 6
#define X509_OBJ_NOT_BEFORE						 8
#define X509_OBJ_NOT_AFTER						 9
#define X509_OBJ_SUBJECT						10
#define X509_OBJ_SUBJECT_PUBLIC_KEY_ALGORITHM	12
#define X509_OBJ_SUBJECT_PUBLIC_KEY				13
#define X509_OBJ_RSA_PUBLIC_KEY					14
#define X509_OBJ_EXTN_ID						22
#define X509_OBJ_CRITICAL						23
#define X509_OBJ_EXTN_VALUE						24
#define X509_OBJ_ALGORITHM						27
#define X509_OBJ_SIGNATURE						28
#define X509_OBJ_ROOF							29


static u_char ASN1_subjectAltName_oid_str[] = {
	0x06, 0x03, 0x55, 0x1D, 0x11
};

static const chunk_t ASN1_subjectAltName_oid = chunk_from_buf(ASN1_subjectAltName_oid_str);


/**
 * compare two X.509 x509s by comparing their signatures
 */
static bool equals(const private_x509_t *this, const private_x509_t *other)
{
	return chunk_equals(this->signature, other->signature);
}

/**
 * extracts the basicConstraints extension
 */
static bool parse_basicConstraints(chunk_t blob, int level0)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	bool isCA = FALSE;

	asn1_init(&ctx, blob, level0, FALSE, FALSE);

	while (objectID < BASIC_CONSTRAINTS_ROOF) {

		if (!extract_object(basicConstraintsObjects, &objectID, &object,&level, &ctx))
		{
			break;
		}
		if (objectID == BASIC_CONSTRAINTS_CA)
		{
			isCA = object.len && *object.ptr;
			DBG2("  %s", isCA ? "TRUE" : "FALSE");
		}
		objectID++;
	}
	return isCA;
}

/**
 * extracts an otherName
 */
static bool parse_otherName(chunk_t blob, int level0)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	int oid = OID_UNKNOWN;

	asn1_init(&ctx, blob, level0, FALSE, FALSE);

	while (objectID < ON_OBJ_ROOF)
	{
		if (!extract_object(otherNameObjects, &objectID, &object, &level, &ctx))
			return FALSE;

		switch (objectID)
		{
			case ON_OBJ_ID_TYPE:
				oid = known_oid(object);
				break;
			case ON_OBJ_VALUE:
				if (oid == OID_XMPP_ADDR)
				{
					if (!parse_asn1_simple_object(&object, ASN1_UTF8STRING, level + 1, "xmppAddr"))
						return FALSE;
				}
				break;
			default:
				break;
		}
		objectID++;
	}
	return TRUE;
}

/**
 * extracts a generalName
 */
static identification_t *parse_generalName(chunk_t blob, int level0)
{
	asn1_ctx_t ctx;
	chunk_t object;
	int objectID = 0;
	u_int level;

	asn1_init(&ctx, blob, level0, FALSE, FALSE);

	while (objectID < GN_OBJ_ROOF)
	{
		id_type_t id_type = ID_ANY;
	
		if (!extract_object(generalNameObjects, &objectID, &object, &level, &ctx))
			return NULL;

		switch (objectID)
		{
			case GN_OBJ_RFC822_NAME:
				id_type = ID_RFC822_ADDR;
				break;
			case GN_OBJ_DNS_NAME:
				id_type = ID_FQDN;
				break;
			case GN_OBJ_URI:
				id_type = ID_DER_ASN1_GN_URI;
				break;
			case GN_OBJ_DIRECTORY_NAME:
				id_type = ID_DER_ASN1_DN;
	    		break;
			case GN_OBJ_IP_ADDRESS:
				id_type = ID_IPV4_ADDR;
				break;
			case GN_OBJ_OTHER_NAME:
				if (!parse_otherName(object, level + 1))
					return NULL;
				break;
			case GN_OBJ_X400_ADDRESS:
			case GN_OBJ_EDI_PARTY_NAME:
			case GN_OBJ_REGISTERED_ID:
				break;
			default:
				break;
		}

		if (id_type != ID_ANY)
		{
			identification_t *gn = identification_create_from_encoding(id_type, object);
			DBG2("  '%D'", gn);
			return gn;
        }
		objectID++;
    }
    return NULL;
}


/*
 * Defined in header.
 */
void x509_parse_generalNames(chunk_t blob, int level0, bool implicit, linked_list_t *list)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;

	asn1_init(&ctx, blob, level0, implicit, FALSE);

	while (objectID < GENERAL_NAMES_ROOF)
	{
		if (!extract_object(generalNamesObjects, &objectID, &object, &level, &ctx))
			return;
		
		if (objectID == GENERAL_NAMES_GN)
		{
			identification_t *gn = parse_generalName(object, level+1);

			if (gn != NULL)
				list->insert_last(list, (void *)gn);
		}
		objectID++;
	}
	return;
}

/**
 * extracts a keyIdentifier
 */
static chunk_t parse_keyIdentifier(chunk_t blob, int level0, bool implicit)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	
	asn1_init(&ctx, blob, level0, implicit, FALSE);
	
	extract_object(keyIdentifierObjects, &objectID, &object, &level, &ctx);
	return object;
}

/*
 * Defined in header.
 */
void x509_parse_authorityKeyIdentifier(chunk_t blob, int level0 , chunk_t *authKeyID, chunk_t *authKeySerialNumber)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	
	*authKeyID = chunk_empty;
	*authKeySerialNumber = chunk_empty;

	asn1_init(&ctx, blob, level0, FALSE, FALSE);

	while (objectID < AUTH_KEY_ID_ROOF)
	{
		if (!extract_object(authorityKeyIdentifierObjects, &objectID, &object, &level, &ctx))
		{
			return;
		}
		switch (objectID) 
		{
			case AUTH_KEY_ID_KEY_ID:
				*authKeyID = parse_keyIdentifier(object, level+1, TRUE);
				break;
			case AUTH_KEY_ID_CERT_ISSUER:
			{
				/* TODO: parse_generalNames(object, level+1, TRUE); */
				break;
			}
			case AUTH_KEY_ID_CERT_SERIAL:
				*authKeySerialNumber = object;
				break;
			default:
				break;
		}
		objectID++;
	}
}

/**
 * extracts an authorityInfoAcess location
 */
static void parse_authorityInfoAccess(chunk_t blob, int level0, linked_list_t *list)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	int accessMethod = OID_UNKNOWN;
	
	asn1_init(&ctx, blob, level0, FALSE, FALSE);
	while (objectID < AUTH_INFO_ACCESS_ROOF)
	{
		if (!extract_object(authorityInfoAccessObjects, &objectID, &object, &level, &ctx))
		{
			return;
		}
		switch (objectID) 
		{
			case AUTH_INFO_ACCESS_METHOD:
				accessMethod = known_oid(object);
				break;
			case AUTH_INFO_ACCESS_LOCATION:
			{
				switch (accessMethod)
				{
					case OID_OCSP:
					case OID_CA_ISSUERS:
						{
							identification_t *accessLocation;

							accessLocation = parse_generalName(object, level+1);
							if (accessLocation == NULL)
							{
								/* parsing went wrong - abort */
								return;
							}
							DBG2("  '%D'", accessLocation);
							if (accessMethod == OID_OCSP)
							{
								list->insert_last(list, (void *)accessLocation);
							}
							else
							{
								/* caIsssuer accessLocation is not used yet */
								accessLocation->destroy(accessLocation);
							}
						}
						break;
					default:
						/* unkown accessMethod, ignoring */
						break;
				}
				break;
			}
			default:
				break;
		}
		objectID++;
	}
}

/**
 * extracts extendedKeyUsage OIDs
 */
static bool parse_extendedKeyUsage(chunk_t blob, int level0)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	
	asn1_init(&ctx, blob, level0, FALSE, FALSE);
	while (objectID < EXT_KEY_USAGE_ROOF)
	{
		if (!extract_object(extendedKeyUsageObjects, &objectID, &object, &level, &ctx))
		{
			return FALSE;
		}
		if (objectID == EXT_KEY_USAGE_PURPOSE_ID && 
			known_oid(object) == OID_OCSP_SIGNING)
		{
			return TRUE;
		}
		objectID++;
	}
	return FALSE;
}

/**
 * extracts one or several crlDistributionPoints and puts them into
 * a chained list
 */
static void parse_crlDistributionPoints(chunk_t blob, int level0, linked_list_t *list)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;
	
	asn1_init(&ctx, blob, level0, FALSE, FALSE);
	while (objectID < CRL_DIST_POINTS_ROOF)
	{
		if (!extract_object(crlDistributionPointsObjects, &objectID, &object, &level, &ctx))
		{
			return;
		}
		if (objectID == CRL_DIST_POINTS_FULLNAME)
		{
			/* append extracted generalNames to existing chained list */
			x509_parse_generalNames(object, level+1, TRUE, list);

		}
		objectID++;
	}
}


/**
 * Parses an X.509v3 certificate
 */
static bool parse_certificate(chunk_t blob, u_int level0, private_x509_t *this)
{
	asn1_ctx_t ctx;
	bool critical;
	chunk_t object;
	u_int level;
	int objectID = 0;
	int extn_oid = OID_UNKNOWN;
	
	asn1_init(&ctx, blob, level0, FALSE, FALSE);
	while (objectID < X509_OBJ_ROOF)
	{
		if (!extract_object(certObjects, &objectID, &object, &level, &ctx))
		{
			return FALSE;
		}

		/* those objects which will parsed further need the next higher level */
		level++;

		switch (objectID)
		{
			case X509_OBJ_CERTIFICATE:
				this->certificate = object;
				break;
			case X509_OBJ_TBS_CERTIFICATE:
				this->tbsCertificate = object;
				break;
			case X509_OBJ_VERSION:
				this->version = (object.len) ? (1+(u_int)*object.ptr) : 1;
				DBG2("  v%d", this->version);
				break;
			case X509_OBJ_SERIAL_NUMBER:
				this->serialNumber = object;
				break;
			case X509_OBJ_SIG_ALG:
				this->signatureAlgorithm = parse_algorithmIdentifier(object, level, NULL);
				break;
			case X509_OBJ_ISSUER:
				this->issuer = identification_create_from_encoding(ID_DER_ASN1_DN, object);
				DBG2("  '%D'", this->issuer);
				break;
			case X509_OBJ_NOT_BEFORE:
				this->notBefore = parse_time(object, level);
				break;
			case X509_OBJ_NOT_AFTER:
				this->notAfter = parse_time(object, level);
				break;
			case X509_OBJ_SUBJECT:
				this->subject = identification_create_from_encoding(ID_DER_ASN1_DN, object);
				DBG2("  '%D'", this->subject);
				break;
			case X509_OBJ_SUBJECT_PUBLIC_KEY_ALGORITHM:
				if (parse_algorithmIdentifier(object, level, NULL) != OID_RSA_ENCRYPTION)
				{
					DBG1("  unsupported public key algorithm");
					return FALSE;
				}
				break;
			case X509_OBJ_SUBJECT_PUBLIC_KEY:
				if (ctx.blobs[4].len > 0 && *ctx.blobs[4].ptr == 0x00)
				{
					/* skip initial bit string octet defining 0 unused bits */
					ctx.blobs[4].ptr++; ctx.blobs[4].len--;
				}
				else
				{
					DBG1("  invalid RSA public key format");
					return FALSE;
				}
				break;
			case X509_OBJ_RSA_PUBLIC_KEY:
				this->subjectPublicKey = object;
				break;
			case X509_OBJ_EXTN_ID:
				extn_oid = known_oid(object);
				break;
			case X509_OBJ_CRITICAL:
				critical = object.len && *object.ptr;
				DBG2("  %s", critical ? "TRUE" : "FALSE");
				break;
			case X509_OBJ_EXTN_VALUE:
			{
				switch (extn_oid)
				{
					case OID_SUBJECT_KEY_ID:
						this->subjectKeyID = chunk_clone(parse_keyIdentifier(object, level, FALSE));
						break;
					case OID_SUBJECT_ALT_NAME:
						x509_parse_generalNames(object, level, FALSE, this->subjectAltNames);
						break;
					case OID_BASIC_CONSTRAINTS:
						this->isCA = parse_basicConstraints(object, level);
						break;
					case OID_CRL_DISTRIBUTION_POINTS:
						parse_crlDistributionPoints(object, level, this->crlDistributionPoints);
						break;
					case OID_AUTHORITY_KEY_ID:
						x509_parse_authorityKeyIdentifier(object, level,
							 &this->authKeyID, &this->authKeySerialNumber);
						break;
					case OID_AUTHORITY_INFO_ACCESS:
						parse_authorityInfoAccess(object, level, this->ocspAccessLocations);
						break;
					case OID_EXTENDED_KEY_USAGE:
						this->isOcspSigner = parse_extendedKeyUsage(object, level);
						break;
					case OID_NS_REVOCATION_URL:
					case OID_NS_CA_REVOCATION_URL:
					case OID_NS_CA_POLICY_URL:
					case OID_NS_COMMENT:
						if (!parse_asn1_simple_object(&object, ASN1_IA5STRING , level, oid_names[extn_oid].name))
							return FALSE;
						break;
					default:
						break;
				}
				break;
			}
			case X509_OBJ_ALGORITHM:
				{
					int alg = parse_algorithmIdentifier(object, level, NULL);

					if (alg != this->signatureAlgorithm)
					{
						DBG1("  signature algorithms do not agree");
						return FALSE;
					}
				}
				break;
			case X509_OBJ_SIGNATURE:
				this->signature = object;
				break;
			default:
				break;
		}
		objectID++;
	}

	/* generate the subjectKeyID if it is missing in the certificate */
	if (this->subjectKeyID.ptr == NULL)
	{
		hasher_t *hasher = hasher_create(HASH_SHA1);

		hasher->allocate_hash(hasher, this->subjectPublicKey, &this->subjectKeyID);
		hasher->destroy(hasher);
	}

	this->installed = time(NULL);
	return TRUE;
}

/**
 * Implements x509_t.is_valid
 */
static err_t is_valid(const private_x509_t *this, time_t *until)
{
	time_t current_time = time(NULL);
	
	DBG2("  not before  : %T", &this->notBefore);
	DBG2("  current time: %T", &current_time);
	DBG2("  not after   : %T", &this->notAfter);

	if (until != NULL &&
		(*until == UNDEFINED_TIME || this->notAfter < *until))
	{
		*until = this->notAfter;
	}
	if (current_time < this->notBefore)
	{
		return "is not valid yet";
	}
	if (current_time > this->notAfter)
	{
		return "has expired";
	}
	DBG2("  certificate is valid");
	return NULL;
}

/**
 * Implements x509_t.is_ca
 */
static bool is_ca(const private_x509_t *this)
{
	return this->isCA;
}

/**
 * Implements x509_t.is_ocsp_signer
 */
static bool is_ocsp_signer(const private_x509_t *this)
{
	return this->isOcspSigner;
}

/**
 * Implements x509_t.is_self_signed
 */
static bool is_self_signed(const private_x509_t *this)
{
	return this->isSelfSigned;
}

/**
 * Implements x509_t.equals_subjectAltName
 */
static bool equals_subjectAltName(const private_x509_t *this, identification_t *id)
{
	bool found = FALSE;
	identification_t *subjectAltName;
	iterator_t *iterator;
	
	iterator = this->subjectAltNames->create_iterator(this->subjectAltNames, TRUE);
	while (iterator->iterate(iterator, (void**)&subjectAltName))
	{
		if (id->equals(id, subjectAltName))
		{
			found = TRUE;
			break;
		}
	}
	iterator->destroy(iterator);
	return found;
}

/**
 * Implements x509_t.is_issuer
 */
static bool is_issuer(const private_x509_t *this, const private_x509_t *issuer)
{
	return (this->authKeyID.ptr)
			? chunk_equals(this->authKeyID, issuer->subjectKeyID)
			: (this->issuer->equals(this->issuer, issuer->subject)
			   && chunk_equals_or_null(this->authKeySerialNumber, issuer->serialNumber));
}

/**
 * Implements x509_t.get_certificate
 */
static chunk_t get_certificate(const private_x509_t *this)
{
	return this->certificate;
}

/**
 * Implements x509_t.get_public_key
 */
static rsa_public_key_t *get_public_key(const private_x509_t *this)
{
	return this->public_key;
}

/**
 * Implements x509_t.get_serialNumber
 */
static chunk_t get_serialNumber(const private_x509_t *this)
{
	return this->serialNumber;
}

/**
 * Implements x509_t.get_subjectKeyID
 */
static chunk_t get_subjectKeyID(const private_x509_t *this)
{
	return this->subjectKeyID;
}

/**
 * Implements x509_t.get_keyid
 */
static chunk_t get_keyid(const private_x509_t *this)
{
	return this->public_key->get_keyid(this->public_key);
}

/**
 * Implements x509_t.get_issuer
 */
static identification_t *get_issuer(const private_x509_t *this)
{
	return this->issuer;
}

/**
 * Implements x509_t.get_subject
 */
static identification_t *get_subject(const private_x509_t *this)
{
	return this->subject;
}

/**
 * Implements x509_t.set_ca_info
 */
static void set_ca_info(private_x509_t *this, ca_info_t *ca_info)
{
	this->ca_info = ca_info;
}

/**
 * Implements x509_t.get_ca_info
 */
static ca_info_t *get_ca_info(const private_x509_t *this)
{
	return this->ca_info;
}

/**
 * Implements x509_t.set_until
 */
static void set_until(private_x509_t *this, time_t until)
{
	this->until = until;
}

/**
 * Implements x509_t.get_until
 */
static time_t get_until(const private_x509_t *this)
{
	return this->until;
}

/**
 * Implements x509_t.set_status
 */
static void set_status(private_x509_t *this, cert_status_t status)
{
	this->status = status;
}

/**
 * Implements x509_t.get_status
 */
static cert_status_t get_status(const private_x509_t *this)
{
	return this->status;
}

/**
 * Implements x509_t.add_authority_flags
 */
static void add_authority_flags(private_x509_t *this, u_int flags)
{
	this->authority_flags |= flags;
}

/**
 * Implements x509_t.add_authority_flags
 */
static u_int get_authority_flags(private_x509_t *this)
{
	return this->authority_flags;
}

/**
 * Implements x509_t.has_authority_flag
 */
static bool has_authority_flag(private_x509_t *this, u_int flags)
{
	return (this->authority_flags & flags) != AUTH_NONE;
}

/**
 * Implements x509_t.create_crluri_iterator
 */
static iterator_t *create_crluri_iterator(const private_x509_t *this)
{
	return this->crlDistributionPoints->create_iterator(this->crlDistributionPoints, TRUE);
}

/**
 * Implements x509_t.create_crluri_iterator
 */
static iterator_t *create_ocspuri_iterator(const private_x509_t *this)
{
	return this->ocspAccessLocations->create_iterator(this->ocspAccessLocations, TRUE);
}

/**
 * Implements x509_t.verify
 */
static bool verify(const private_x509_t *this, const rsa_public_key_t *signer)
{
	hash_algorithm_t algorithm = hasher_algorithm_from_oid(this->signatureAlgorithm);

	if (algorithm == HASH_UNKNOWN)
	{
		DBG1("  unknown signature algorithm");
		return FALSE;
	}
	return signer->verify_emsa_pkcs1_signature(signer, algorithm, this->tbsCertificate, this->signature) == SUCCESS;
}
	
/**
 * Implementation of x509_t.list.
 */
static void list(private_x509_t *this, FILE *out, bool utc)
{
	iterator_t *iterator;
	time_t now = time(NULL);

	fprintf(out, "%#T\n", &this->installed, utc);

	if (this->subjectAltNames->get_count(this->subjectAltNames))
	{
		identification_t *subjectAltName;
		bool first = TRUE;

		fprintf(out, "    altNames:  ");
		iterator = this->subjectAltNames->create_iterator(this->subjectAltNames, TRUE);
		while (iterator->iterate(iterator, (void**)&subjectAltName))
		{
			if (first)
			{
				first = FALSE;
			}
			else
			{
				fprintf(out, ", ");
			}
			fprintf(out, "'%D'", subjectAltName);
		}
		iterator->destroy(iterator);
		fprintf(out, "\n");
	}
	fprintf(out, "    subject:   '%D'\n", this->subject);
	fprintf(out, "    issuer:    '%D'\n", this->issuer);
	fprintf(out, "    serial:     %#B\n", &this->serialNumber);
	fprintf(out, "    validity:   not before %#T, ", &this->notBefore, utc);
	if (now < this->notBefore)
	{
		fprintf(out, "not valid yet (valid in %#V)\n", &now, &this->notBefore);
	}
	else
	{
		fprintf(out, "ok\n");
	}
	
	fprintf(out, "                not after  %#T, ", &this->notAfter, utc);
	if (now > this->notAfter)
	{
		fprintf(out, "expired (%#V ago)\n", &now, &this->notAfter);
	}
	else
	{
		fprintf(out, "ok");
		if (now > this->notAfter - CERT_WARNING_INTERVAL * 60 * 60 * 24)
		{
			fprintf(out, " (expires in %#V)", &now, &this->notAfter);
		}
		fprintf(out, " \n");
	}
	
	{
		chunk_t keyid = this->public_key->get_keyid(this->public_key);
		fprintf(out, "    keyid:      %#B\n", &keyid);
	}

	if (this->subjectKeyID.ptr)
	{
		fprintf(out, "    subjkey:    %#B\n", &this->subjectKeyID);
	}
	if (this->authKeyID.ptr)
	{
		fprintf(out, "    authkey:    %#B\n", &this->authKeyID);
	}
	if (this->authKeySerialNumber.ptr)
	{
		fprintf(out, "    aserial:    %#B\n", &this->authKeySerialNumber);
	}
	
	fprintf(out, "    pubkey:     RSA %d bits", BITS_PER_BYTE *
			this->public_key->get_keysize(this->public_key));
	fprintf(out, ", status %N",
		   cert_status_names, this->status);
	
	switch (this->status)
	{
		case CERT_GOOD:
			fprintf(out, " until %#T", &this->until, utc);
			break;
		case CERT_REVOKED:
			fprintf(out, " on %#T", &this->until, utc);
			break;
		case CERT_UNKNOWN:
		case CERT_UNDEFINED:
		case CERT_UNTRUSTED:
		default:
			break;
	}
}

/*
 * Defined in header.
 */
chunk_t x509_build_generalNames(linked_list_t *list)
{
	linked_list_t *generalNames = linked_list_create();
	iterator_t *iterator = list->create_iterator(list, TRUE);
	identification_t *name;
	size_t len = 0;

	while (iterator->iterate(iterator, (void**)&name))
	{
		asn1_t asn1_type = ASN1_EOC;
		chunk_t *generalName = malloc_thing(chunk_t);

		switch (name->get_type(name))
		{
			case ID_RFC822_ADDR:
				asn1_type = ASN1_CONTEXT_S_1;
				break;
			case ID_FQDN:
				asn1_type = ASN1_CONTEXT_S_2;
				break;
			case ID_DER_ASN1_DN:
				asn1_type = ASN1_CONTEXT_C_4;
	    		break;
			case ID_DER_ASN1_GN_URI:
				asn1_type = ASN1_CONTEXT_S_6;
				break;
			case ID_IPV4_ADDR:
				asn1_type = ASN1_CONTEXT_S_7;
				break;
			default:
				continue;
		}

		*generalName = asn1_simple_object(asn1_type, name->get_encoding(name));
		len += generalName->len;
		generalNames->insert_last(generalNames, generalName);
	}
	iterator->destroy(iterator);

	if (len > 0)
	{
		iterator_t *iterator = generalNames->create_iterator(generalNames, TRUE);
		chunk_t names, *generalName;
		u_char *pos = build_asn1_object(&names, ASN1_SEQUENCE, len);

		while (iterator->iterate(iterator, (void**)&generalName))
		{
			memcpy(pos, generalName->ptr, generalName->len);
			pos += generalName->len;
			free(generalName->ptr);
			free(generalName);
		}
		iterator->destroy(iterator);
		generalNames->destroy(generalNames);

		return asn1_wrap(ASN1_OCTET_STRING, "m", names);
	}
	else
	{
		return chunk_empty;
	}
}

/*
 * Defined in header.
 */
chunk_t x509_build_subjectAltNames(linked_list_t *list)
{
	chunk_t generalNames = x509_build_generalNames(list);

	if (generalNames.len)
	{
		return asn1_wrap(ASN1_SEQUENCE, "cm",
					ASN1_subjectAltName_oid,
					asn1_wrap(ASN1_OCTET_STRING, "m", generalNames)
				);
	}
	else
	{
		return chunk_empty;
	}
}

/**
 * Implementation of x509_t.build_encoding.
 */
static void build_encoding(private_x509_t *this, hash_algorithm_t alg,
						   rsa_private_key_t *private_key)
{

}

/**
 * Implements x509_t.destroy
 */
static void destroy(private_x509_t *this)
{
	this->subjectAltNames->destroy_offset(this->subjectAltNames,
								offsetof(identification_t, destroy));
	this->crlDistributionPoints->destroy_offset(this->crlDistributionPoints,
								offsetof(identification_t, destroy));
	this->ocspAccessLocations->destroy_offset(this->ocspAccessLocations,
								offsetof(identification_t, destroy));
	DESTROY_IF(this->issuer);
	DESTROY_IF(this->subject);
	DESTROY_IF(this->public_key);
	free(this->subjectKeyID.ptr);
	free(this->certificate.ptr);
	free(this);
}

/**
 * Internal generic constructor
 */
static private_x509_t *x509_create_empty(void)
{
	private_x509_t *this = malloc_thing(private_x509_t);
	
	/* initialize */
	this->subjectPublicKey = chunk_empty;
	this->public_key = NULL;
	this->subject = NULL;
	this->issuer = NULL;
	this->ca_info = NULL;
	this->subjectAltNames = linked_list_create();
	this->crlDistributionPoints = linked_list_create();
	this->ocspAccessLocations = linked_list_create();
	this->subjectKeyID = chunk_empty;
	this->authKeyID = chunk_empty;
	this->authKeySerialNumber = chunk_empty;
	this->authority_flags = AUTH_NONE;
	this->isCA = FALSE;
	this->isOcspSigner = FALSE;
	
	/* public functions */
	this->public.equals = (bool (*) (const x509_t*,const x509_t*))equals;
	this->public.equals_subjectAltName = (bool (*) (const x509_t*,identification_t*))equals_subjectAltName;
	this->public.is_issuer = (bool (*) (const x509_t*,const x509_t*))is_issuer;
	this->public.is_valid = (err_t (*) (const x509_t*,time_t*))is_valid;
	this->public.is_ca = (bool (*) (const x509_t*))is_ca;
	this->public.is_self_signed = (bool (*) (const x509_t*))is_self_signed;
	this->public.is_ocsp_signer = (bool (*) (const x509_t*))is_ocsp_signer;
	this->public.get_certificate = (chunk_t (*) (const x509_t*))get_certificate;
	this->public.get_public_key = (rsa_public_key_t* (*) (const x509_t*))get_public_key;
	this->public.get_serialNumber = (chunk_t (*) (const x509_t*))get_serialNumber;
	this->public.get_subjectKeyID = (chunk_t (*) (const x509_t*))get_subjectKeyID;
	this->public.get_keyid = (chunk_t (*) (const x509_t*))get_keyid;
	this->public.get_issuer = (identification_t* (*) (const x509_t*))get_issuer;
	this->public.get_subject = (identification_t* (*) (const x509_t*))get_subject;
	this->public.set_ca_info = (void (*) (x509_t*,ca_info_t*))set_ca_info;
	this->public.get_ca_info = (ca_info_t* (*) (const x509_t*))get_ca_info;
	this->public.set_until = (void (*) (x509_t*,time_t))set_until;
	this->public.get_until = (time_t (*) (const x509_t*))get_until;
	this->public.set_status = (void (*) (x509_t*,cert_status_t))set_status;
	this->public.get_status = (cert_status_t (*) (const x509_t*))get_status;
	this->public.add_authority_flags = (void (*) (x509_t*,u_int))add_authority_flags;
	this->public.get_authority_flags = (u_int (*) (x509_t*))get_authority_flags;
	this->public.has_authority_flag = (bool (*) (x509_t*,u_int))has_authority_flag;
	this->public.create_crluri_iterator = (iterator_t* (*) (const x509_t*))create_crluri_iterator;
	this->public.create_ocspuri_iterator = (iterator_t* (*) (const x509_t*))create_ocspuri_iterator;
	this->public.verify = (bool (*) (const x509_t*,const rsa_public_key_t*))verify;
	this->public.list = (void (*) (x509_t*, FILE *out, bool utc))list;
	this->public.build_encoding = (void (*) (x509_t*,hash_algorithm_t,rsa_private_key_t*))build_encoding;
	this->public.destroy = (void (*) (x509_t*))destroy;
	
	return this;
}

/*
 * Described in header.
 */
x509_t *x509_create_(chunk_t serialNumber, identification_t *issuer, identification_t *subject)
{
	private_x509_t *this = x509_create_empty();

	this->serialNumber = serialNumber;
	this->issuer = issuer->clone(issuer);
	this->subject = subject->clone(subject);

	return &this->public;
}

/*
 * Described in header.
 */
x509_t *x509_create_from_chunk(chunk_t chunk, u_int level)
{
	private_x509_t *this = x509_create_empty();

	if (!parse_certificate(chunk, level, this))
	{
		destroy(this);
		return NULL;
	}

	/* extract public key from certificate */
	this->public_key = rsa_public_key_create_from_chunk(this->subjectPublicKey);
	if (this->public_key == NULL)
	{
		destroy(this);
		return NULL;
	}

	/* set trusted lifetime of public key to notAfter */
	this->until = this->notAfter;

	/* check if the certificate is self-signed */
	this->isSelfSigned = FALSE;
	if (this->subject->equals(this->subject, this->issuer))
	{
		hash_algorithm_t algorithm = hasher_algorithm_from_oid(this->signatureAlgorithm);

		if (algorithm == HASH_UNKNOWN)
		{
			destroy(this);
			return NULL;
		}
		this->isSelfSigned = this->public_key->verify_emsa_pkcs1_signature(this->public_key,
							 algorithm, this->tbsCertificate, this->signature) == SUCCESS;
	}
	if (this->isSelfSigned)
	{
		DBG2("  certificate is self-signed");
		this->status = CERT_GOOD;
	}
	else
	{
		this->status = CERT_UNDEFINED;
	}

	return &this->public;
}

/*
 * Described in header.
 */
x509_t *x509_create_from_file(const char *filename, const char *label)
{
	bool pgp = FALSE;
	chunk_t chunk = chunk_empty;
	char cert_label[BUF_LEN];

	snprintf(cert_label, BUF_LEN, "%s certificate", label);

	if (!pem_asn1_load_file(filename, NULL, cert_label, &chunk, &pgp))
	{
		return NULL;
	}
	return x509_create_from_chunk(chunk, 0);
}
