/****************************************************************************
 *
 *   drvproc.c
 *
 *   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
 *   Copyright (c) 2019-2020 Andrei Warkentin <andrey.warkentin@gmail.com>
 *
 ***************************************************************************/

#include <windows.h>
#include <mmsystem.h>
#include "adlib.h"

/***************************************************************************
 * @doc INTERNAL
 *
 * @func void | DrvLoadInitialize | This is where system.ini can get parsed
 * using GetPrivateProfileInt.
 *
 * @rdesc Nothing.
 ***************************************************************************/
static void PASCAL NEAR
DrvLoadInitialize(void)
{
}

/***************************************************************************
 * @doc INTERNAL
 *
 * @api LONG | DriverProc | The entry point for an installable driver.
 *
 * @parm DWORD | dwDriverId | For most messages, <p dwDriverId> is the DWORD
 *     value that the driver returns in response to a <m DRV_OPEN> message.
 *     Each time that the driver is opened, through the <f DrvOpen> API,
 *     the driver receives a <m DRV_OPEN> message and can return an
 *     arbitrary, non-zero value. The installable driver interface
 *     saves this value and returns a unique driver handle to the
 *     application. Whenever the application sends a message to the
 *     driver using the driver handle, the interface routes the message
 *     to this entry point and passes the corresponding <p dwDriverId>.
 *     This mechanism allows the driver to use the same or different
 *     identifiers for multiple opens but ensures that driver handles
 *     are unique at the application interface layer.
 *
 *     The following messages are not related to a particular open
 *     instance of the driver. For these messages, the dwDriverId
 *     will always be zero.
 *
 *         DRV_LOAD, DRV_FREE, DRV_ENABLE, DRV_DISABLE, DRV_OPEN
 *
 * @parm HANDLE  | hDriver | This is the handle returned to the
 *     application by the driver interface.
 
 * @parm WORD | wMessage | The requested action to be performed. Message
 *     values below <m DRV_RESERVED> are used for globally defined messages.
 *     Message values from <m DRV_RESERVED> to <m DRV_USER> are used for
 *     defined driver protocols. Messages above <m DRV_USER> are used
 *     for driver specific messages.
 *
 * @parm LONG | lParam1 | Data for this message.  Defined separately for
 *     each message
 *
 * @parm LONG | lParam2 | Data for this message.  Defined separately for
 *     each message
 *
 * @rdesc Defined separately for each message.
 ***************************************************************************/
LONG FAR PASCAL _loadds
DriverProc(DWORD dwDriverID,
           HANDLE hDriver,
           WORD wMessage,
           LONG lParam1,
           LONG lParam2)
{
  switch (wMessage) {
  case DRV_LOAD:
    D1("DRV_LOAD");

    /*
     * Sent to the driver when it is loaded. Always the first
     * message received by a driver.
     *
     * dwDriverID is 0L.
     * lParam1 is 0L.
     * lParam2 is 0L.
     *
     * Return 0L to fail the load.
     *
     * DefDriverProc will return NON-ZERO so we don't have to
     * handle DRV_LOAD
     */

    DrvLoadInitialize();
    return 1L;

  case DRV_FREE:
    D1("DRV_FREE");

    /*
     * Sent to the driver when it is about to be discarded. This
     * will always be the last message received by a driver before
     * it is freed.
     *
     * dwDriverID is 0L.
     * lParam1 is 0L.
     * lParam2 is 0L.
     * Return value is ignored.
     */
    return 1L;

  case DRV_OPEN:
    D1("DRV_OPEN");

    /*
     * Sent to the driver when it is opened.
     *
     * dwDriverID is 0L.
     *
     * lParam1 is a far pointer to a zero-terminated string
     * containing the name used to open the driver.
     *
     * lParam2 is passed through from the drvOpen call.
     *
     * Return 0L to fail the open.
     *
     * DefDriverProc will return ZERO so we do have to
     * handle the DRV_OPEN message.
     */
    return 1L;

  case DRV_CLOSE:
    D1("DRV_CLOSE");

    /*
     * Sent to the driver when it is closed. Drivers are unloaded
     * when the close count reaches zero.
     *
     * dwDriverID is the driver identifier returned from the
     * corresponding DRV_OPEN.
     *
     * lParam1 is passed through from the drvClose call.
     *
     * lParam2 is passed through from the drvClose call.
     *
     * Return 0L to fail the close.
     *
     * DefDriverProc will return ZERO so we do have to
     * handle the DRV_CLOSE message.
     */
    return 1L;

  case DRV_ENABLE:
    D1("DRV_ENABLE");

    /*
     * Sent to the driver when the driver is loaded or reloaded
     * and whenever Windows is enabled. Drivers should only
     * hook interrupts or expect ANY part of the driver to be in
     * memory between enable and disable messages
     *
     * dwDriverID is 0L.
     * lParam1 is 0L.
     * lParam2 is 0L.
     *
     * Return value is ignored.
     */
    return Enable() ? 1L : 0L;

  case DRV_DISABLE:
    D1("DRV_DISABLE");

    /*
     * Sent to the driver before the driver is freed.
     * and whenever Windows is disabled
     *
     * dwDriverID is 0L.
     * lParam1 is 0L.
     * lParam2 is 0L.
     *
     * Return value is ignored.
     */
    Disable();
    return 1L;

  case DRV_QUERYCONFIGURE:
    D1("DRV_QUERYCONFIGURE");

    /*
     * Sent to the driver so that applications can
     * determine whether the driver supports custom
     * configuration. The driver should return a
     * non-zero value to indicate that configuration
     * is supported.
     *
     * dwDriverID is the value returned from the DRV_OPEN
     * call that must have succeeded before this message
     * was sent.
     *
     * lParam1 is passed from the app and is undefined.
     * lParam2 is passed from the app and is undefined.
     *
     * Return 0L to indicate configuration NOT supported.
     */
    return 0L;        /* we don't do configuration */

  case DRV_CONFIGURE:
    D1("DRV_CONFIGURE");

    /*
     * Sent to the driver so that it can display a custom
     * configuration dialog box.
     *
     * lParam1 is passed from the app. and should contain
     * the parent window handle in the loword.
     * lParam2 is passed from the app and is undefined.
     *
     * Return value is undefined.
     *
     * Drivers should create their own section in system.ini.
     * The section name should be the driver name.
     */
    return DRVCNF_CANCEL;

  case DRV_INSTALL:
    D1("DRV_INSTALL");
    return DRVCNF_RESTART;

  default:
    return DefDriverProc(dwDriverID, hDriver, wMessage, lParam1, lParam2);
  }
}
