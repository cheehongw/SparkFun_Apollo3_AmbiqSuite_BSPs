
#include <string.h>
#include "am_mcu_apollo.h"
#include "wsf_types.h"
#include "wsf_assert.h"
#include "util/bda.h"
#include "app_api.h"
#include "app_main.h"
#include "app_db.h"
#include "app_cfg.h"

typedef struct {
  /*! Common for all roles */
  bdAddr_t     peerAddr;                      /*! Peer address */
  uint8_t      addrType;                      /*! Peer address type */
  dmSecIrk_t   peerIrk;                       /*! Peer IRK */
  dmSecCsrk_t  peerCsrk;                      /*! Peer CSRK */
  uint8_t      keyValidMask;                  /*! Valid keys in this record */
  bool_t       inUse;                         /*! TRUE if record in use */
  bool_t       valid;                         /*! TRUE if record is valid */
  bool_t       peerAddedToRl;                 /*! TRUE if peer device's been added to resolving list */
  bool_t       peerRpao;                      /*! TRUE if RPA Only attribute's present on peer device */

  /*! For slave local device */
  dmSecLtk_t   localLtk;                      /*! Local LTK */
  uint8_t      localLtkSecLevel;              /*! Local LTK security level */
  bool_t       peerAddrRes;                   /*! TRUE if address resolution's supported on peer device (master) */

  /*! For master local device */
  dmSecLtk_t   peerLtk;                       /*! Peer LTK */
  uint8_t      peerLtkSecLevel;               /*! Peer LTK security level */

  /*! for ATT server local device */
  uint16_t     cccTbl[APP_DB_NUM_CCCD];       /*! Client characteristic configuration descriptors */
  uint32_t     peerSignCounter;               /*! Peer Sign Counter */
  uint8_t      changeAwareState;              /*! Peer client awareness to state change in database */
  uint8_t      csf[ATT_CSF_LEN];              /*! Peer client supported features record */

  /*! for ATT client */
  bool_t       cacheByHash;                   /*! TRUE if cached handles are maintained by comparing database hash */
  uint8_t      dbHash[ATT_DATABASE_HASH_LEN]; /*! Peer database hash */
  uint16_t     hdlList[APP_DB_HDL_LIST_LEN];  /*! Cached handle list */
  uint8_t      discStatus;                    /*! Service discovery and configuration status */
  bool_t       master_role;                   /*! True if local device is master for this record */
#ifdef AM_VOS_SDK
  uint32_t     dummyReserved[3];              // Need to review!! (16 bytes align for flash write.)
#endif // AM_VOS_SDK
} appDbRec_t;