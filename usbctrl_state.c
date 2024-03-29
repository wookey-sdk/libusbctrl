#include "libc/types.h"
#include "libc/stdio.h"
#include "libc/sync.h"
#include "usbctrl.h"
#include "usbctrl_state.h"
#include "usbctrl_requests.h"
#ifdef __FRAMAC__
#ifdef FRAMAC_WITH_META
/* another function is allowed to manipulate the USB state: framac_state_manipulator, from the FramaC emulated entrypoint.
 * This function is NOT a part of the library but used to stress out the various error cases and branches of the lib
 */
#include "framac/entrypoint.h"
#endif
#endif


/*
 * all allowed transitions and target states for each current state
 * empty fields are set as 0xff/0xff for request/state couple, which is
 * an inexistent state and request
 *
 * This table associate each state of the DFU automaton with up to
 * 5 potential allowed transitions/next_state couples. This permit to
 * easily detect:
 *    1) authorized transitions, based on the current state
 *    2) next state, based on the current state and current transition
 *
 * If the next_state for the current transision is keeped to 0xff, this
 * means that the current transition for the current state may lead to
 * multiple next state depending on other informations. In this case,
 * the transition handler has to handle this manually.
 */

#ifdef __FRAMAC__
#ifdef FRAMAC_WITH_META
/*
 * This is the USB control plane state automaton conformity proof set against USB 2.0
 * specifications.
 * The proofs are based on MetaCSL high level specifications that guarantee that
 * automaton states and transitions are valid and comply with the specifications.
 *
 * We wish to thank to Virgile Prevosto case study on the Wookey bootloader on
 * which the current automaton metACSL content is based.
 */

/* ==> The automaton state is only writeable by usbctrl_set_state() */

#define VALID_STATE(i) (ctx_list[i].state >= USB_DEVICE_STATE_ATTACHED && ctx_list[i].state < USB_DEVICE_STATE_INVALID)
#define STATE_UNTOUCHED(i) (ctx_list[i].state == \old(ctx_list[i].state))

/*@

   meta \prop,
     \name(state_only_changed_in_set_state),
     \targets(\diff(\ALL, usbctrl_set_state)),
     \context(\writing),
     \separated(\written, &ctx_list[0 .. CONFIG_USBCTRL_MAX_CTX-1].state);

	// only usbctrl_set_state() is allowed to update ctx_list[].state
    // in the Frama-C framework context, there is two control plane contexts (i.e. CONFIG_USBCTRL_MAX_CTX == 2)
    meta \prop,
           \name(ctx0_state_controled_update),
           \targets(\diff(\ALL, \union(usbctrl_set_state, \callers(usbctrl_set_state)))),
           \context(\postcond),
           \flags(proof:deduce),
           \fguard(STATE_UNTOUCHED(0));
    meta \prop,
           \name(ctx1_state_controled_update),
           \targets(\diff(\ALL, \union(usbctrl_set_state, \callers(usbctrl_set_state)))),
           \context(\postcond),
           \flags(proof:deduce),
           \fguard(STATE_UNTOUCHED(1));

*/

/* ==> The automaton state is always valid */


/*@ meta \prop,
        \name(state_always_valid_small),
        \targets(usbctrl_set_state),
        \context(\strong_invariant),
        \flags(proof:axiom, translate:no),
        (VALID_STATE(0) && VALID_STATE(1));
*/

/*@ meta \prop,
        \name(state_always_valid_callees),
        \targets(\callees(usbctrl_set_state)),
        \context(\strong_invariant),
        \flags(proof:deduce, translate:no),
        (VALID_STATE(0) && VALID_STATE(1));
*/

/*@ meta \prop,
        \name(state_always_valid),
        \targets(\ALL),
        \context(\weak_invariant),
        \flags(proof:deduce, translate:yes),
        (VALID_STATE(0) && VALID_STATE(1));
*/

/* ==> Restrict transition to transition functions only */

/*
 * Definition of the functions set that is responsible of
 * automaton transitions
 * usbctrl_start_device activate the HW IP and thus set the state automaton
 * from 'ATTACHED' (default) to 'POWERED'. Other are USB events.
 */
#define TRANSITION_FUNCTIONS ({              \
    usbctrl_start_device,                    \
    usbctrl_stop_device,                     \
    usbctrl_std_req_handle_set_address,      \
    usbctrl_std_req_handle_set_configuration,\
    usbctrl_handle_usbsuspend,               \
    usbctrl_handle_reset,                    \
    usbctrl_handle_wakeup                    \
})


/* this proof is to be translated to ACSL for provers, not deduced */

/* ==> only transiion functions (and framac manipulator, out of the library) is allowed to call usbctrl_set_state */

/*@ meta \prop,
        \name(state_wrapper_only_called_in_transitions),
        \targets(\diff(\ALL,
                    \union(framac_state_manipulator,
                    TRANSITION_FUNCTIONS))),
        \context(\calling),
        \tguard(\called != usbctrl_set_state);
*/

/* ==> for each target state, specify from which state the automaton allows a transition
 *
 * These successive meta properties specify, for each target state, allowed source states
 * the USB 2.0 standard allows in the USB state automaton specifications.
 *
 * These meta-properties **does not** specify which transition function is responsible of
 * the transition, only the origin and the destination of the transition itself.
 */

/*@ meta \prop,
        \name(transition_allowed_toward_attached),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_ATTACHED ==>
         (
           \at(ctx->state,Before) == USB_DEVICE_STATE_POWERED ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_DEFAULT ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_ADDRESS ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_CONFIGURED ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_DEFAULT ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_ADDRESS ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_CONFIGURED
         ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_suspended_powered),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_POWERED ==>
         (
           \at(ctx->state,Before) == USB_DEVICE_STATE_ATTACHED
         ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_default),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_DEFAULT ==>
         (
           \at(ctx->state,Before) == USB_DEVICE_STATE_POWERED ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_DEFAULT ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_ADDRESS ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_CONFIGURED ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_DEFAULT ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_ADDRESS ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_CONFIGURED
         ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_address),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_ADDRESS ==>
           (
           \at(ctx->state,Before) == USB_DEVICE_STATE_DEFAULT ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_CONFIGURED ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_ADDRESS
           ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_configured),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_CONFIGURED ==>
           (
           \at(ctx->state,Before) == USB_DEVICE_STATE_ADDRESS ||
           \at(ctx->state,Before) == USB_DEVICE_STATE_SUSPENDED_CONFIGURED
           ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_suspended_default),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_SUSPENDED_DEFAULT ==>
         (
           \at(ctx->state,Before) == USB_DEVICE_STATE_DEFAULT
         ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_suspended_address),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_SUSPENDED_ADDRESS ==>
         (
           \at(ctx->state,Before) == USB_DEVICE_STATE_ADDRESS
         ));
*/
/*@ meta \prop,
        \name(transition_allowed_toward_suspended_configured),
        \targets(usbctrl_set_state),
        \context(\precond),
        \tguard(newstate == USB_DEVICE_STATE_SUSPENDED_CONFIGURED ==>
         (
           \at(ctx->state,Before) == USB_DEVICE_STATE_CONFIGURED
         ));
*/


/* ==> Restrict transition functions target states to allowed ones only
 *
 * Here we associate each transition to dedicated transition function.
 * Only thr target state is specified, but with previous meta-properties,
 * each target state is associated to a list of allowed source states.
 */

/*
 * INFO: the following metaproperties require metACSL v0.1.2 or higher.
 * They are not activated by default.
 *
 */
#ifdef WITH_META_CALLARG_SUPPORT

/*@
    meta \prop,
        \name(set_address_can_only_set_allowed_states),
        \targets(usbctrl_std_req_handle_set_address),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
            \called_arg(newstate) == USB_DEVICE_STATE_DEFAULT ||
            \called_arg(newstate) == USB_DEVICE_STATE_ADDRESS));
*/

/*@
    meta \prop,
        \name(set_configuration_can_only_set_allowed_states),
        \targets(usbctrl_std_req_handle_set_configuration),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
            \called_arg(newstate) == USB_DEVICE_STATE_CONFIGURED ||
            \called_arg(newstate) == USB_DEVICE_STATE_ADDRESS));
*/

/*@
    meta \prop,
        \name(usbsuspend_can_only_set_allowed_states),
        \targets(usbctrl_handle_usbsuspend),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
           \called_arg(newstate) == USB_DEVICE_STATE_SUSPENDED_POWER ||
           \called_arg(newstate) == USB_DEVICE_STATE_SUSPENDED_DEFAULT ||
           \called_arg(newstate) == USB_DEVICE_STATE_SUSPENDED_ADDRESS ||
           \called_arg(newstate) == USB_DEVICE_STATE_SUSPENDED_CONFIGURED));
*/

/*@
    meta \prop,
        \name(usbwakeup_can_only_set_allowed_states),
        \targets(usbctrl_handle_wakeup),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
           \called_arg(newstate) == USB_DEVICE_STATE_POWERED ||
           \called_arg(newstate) == USB_DEVICE_STATE_DEFAULT ||
           \called_arg(newstate) == USB_DEVICE_STATE_ADDRESS ||
           \called_arg(newstate) == USB_DEVICE_STATE_CONFIGURED));
*/

/*@
    meta \prop,
        \name(reset_can_only_set_allowed_states),
        \targets(usbctrl_handle_reset),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
           \called_arg(newstate) == USB_DEVICE_STATE_DEFAULT));
*/

/*@
    meta \prop,
        \name(start_device_can_only_set_allowed_states),
        \targets(usbctrl_start_device),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
           \called_arg(newstate) == USB_DEVICE_STATE_POWERED));
*/

/*@
    meta \prop,
        \name(stop_device_can_only_set_allowed_states),
        \targets(usbctrl_stop_device),
        \context(\calling),
        \tguard(\called == usbctrl_set_state ==> \fguard(
           \called_arg(newstate) == USB_DEVICE_STATE_ATTACHED));
*/
#endif/*WITH_META_CALLARG_SUPPORT */


#endif/*!FRAMAC_WITH_META */
#endif/*!__FRAMAC__*/


#ifndef __FRAMAC__
 /*
    All variables declared here are reported in state.h, so that they can be used in other function specifications (in other files)
 */

/*
 * Association between a request and a transition to a next state. This couple
 * depend on the current state and is use in the following structure
 */


#define MAX_TRANSITION_STATE 10
typedef struct usb_operation_code_transition {
    uint8_t request;
    uint8_t target_state;
} usb_request_code_transition_t;


static const struct {
    usb_device_state_t state;
    usb_request_code_transition_t   req_trans[MAX_TRANSITION_STATE];
} usb_automaton[] = {
    {USB_DEVICE_STATE_ATTACHED, {
                 {USB_DEVICE_TRANS_HUB_CONFIGURED, USB_DEVICE_STATE_POWERED},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 {0xff, 0xff},
                 }
     },
    {USB_DEVICE_STATE_POWERED, {
                  {USB_DEVICE_TRANS_BUS_INACTIVE, USB_DEVICE_STATE_SUSPENDED_POWER},
                  {USB_DEVICE_TRANS_HUB_RESET, USB_DEVICE_STATE_ATTACHED},
                  {USB_DEVICE_TRANS_HUB_DECONFIGURED, USB_DEVICE_STATE_ATTACHED},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                 }
     },
    {USB_DEVICE_STATE_SUSPENDED_POWER, {
                  {USB_DEVICE_TRANS_BUS_ACTIVE, USB_DEVICE_STATE_POWERED},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  }
     },
    {USB_DEVICE_STATE_SUSPENDED_DEFAULT, {
                  {USB_DEVICE_TRANS_BUS_ACTIVE, USB_DEVICE_STATE_DEFAULT},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  }
     },
    {USB_DEVICE_STATE_SUSPENDED_ADDRESS, {
                  {USB_DEVICE_TRANS_BUS_ACTIVE, USB_DEVICE_STATE_ADDRESS},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  }
     },
    {USB_DEVICE_STATE_SUSPENDED_CONFIGURED, {
                  {USB_DEVICE_TRANS_BUS_ACTIVE, USB_DEVICE_STATE_CONFIGURED},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  }
     },
    {USB_DEVICE_STATE_DEFAULT, {
                  {USB_DEVICE_TRANS_ADDRESS_ASSIGNED, USB_DEVICE_STATE_ADDRESS},
                  {USB_DEVICE_TRANS_BUS_INACTIVE, USB_DEVICE_STATE_SUSPENDED_DEFAULT},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  },
     },
    {USB_DEVICE_STATE_ADDRESS, {
                  {USB_DEVICE_TRANS_DEV_CONFIGURED, USB_DEVICE_STATE_CONFIGURED},
                  {USB_DEVICE_TRANS_BUS_INACTIVE, USB_DEVICE_STATE_SUSPENDED_ADDRESS},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  },
     },
    {USB_DEVICE_STATE_CONFIGURED, {
                  {USB_DEVICE_TRANS_DEV_DECONFIGURED, USB_DEVICE_STATE_ADDRESS},
                  {USB_DEVICE_TRANS_BUS_INACTIVE, USB_DEVICE_STATE_SUSPENDED_CONFIGURED},
                  {USB_DEVICE_TRANS_RESET, USB_DEVICE_STATE_DEFAULT},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  },
     },
    {USB_DEVICE_STATE_INVALID, { // should never be set
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  {0xff, 0xff},
                  },
     },

};

#endif/*__FRAMAC__*/

/**********************************************
 * USB CTRL State automaton getters and setters
 *********************************************/


/*@
  @ assigns \nothing ;
  @ ensures (ctx == \null) ==> \result == USB_DEVICE_STATE_INVALID ;
  @ ensures (ctx != \null) ==> \result == ctx->state ;
*/

usb_device_state_t usbctrl_get_state(const usbctrl_context_t *ctx)
{
   if (ctx == NULL) {
       return USB_DEVICE_STATE_INVALID;
   }
   return ctx->state;
}

/*
 * This function is eligible in both main thread and ISR
 * mode (through trigger execution). Please use aprintf only
 * here.
 */

/*@
  @ assigns ctx->state ;

  @ behavior invparam:
  @   assumes (newstate >= USB_DEVICE_STATE_INVALID || ctx == \null);
  @   ensures ctx->state == \old(ctx->state);
  @   ensures \result == MBED_ERROR_INVPARAM ;

  @ behavior ok:
  @   assumes !(newstate >= USB_DEVICE_STATE_INVALID || ctx == \null);
  @   ensures ctx->state == newstate;
  @   ensures \result == MBED_ERROR_NONE;

  @ disjoint behaviors;
  @ complete behaviors;
*/

mbed_error_t usbctrl_set_state(__out usbctrl_context_t *ctx,
                               __in usb_device_state_t newstate)
{
    /* FIXME: transient, maybe we need to lock here. */
   if (ctx == NULL) {
       return MBED_ERROR_INVPARAM;
   }
    if (newstate >= USB_DEVICE_STATE_INVALID) {
        log_printf("[USBCTRL] invalid state transition !\n");
        return MBED_ERROR_INVPARAM;
    }
    log_printf("[USBCTRL] changing from state %x to %x\n", ctx->state, newstate);
    /*@ assert \valid(&ctx->state); */
    /* here we do not use set_u8_with_membarrier() because it forbid metACSL from detecting that
     * ctx->state is assigned only here. Instead, using request_data_membarrier() */
    ctx->state = newstate;
    request_data_membarrier();

    return MBED_ERROR_NONE;
}


/******************************************************
 * USBCTRL automaton management function (transition and
 * state check)
 *****************************************************/

/*!
 * \brief return the next automaton state
 *
 * The next state is returned depending on the current state
 * and the current request. In some case, it can be 0xff if multiple
 * next states are possible.
 *
 * \param current_state the current automaton state
 * \param request       the current transition request
 *
 * \return the next state, or 0xff
 */

/*@
  @ requires is_valid_state(current_state);
  @ requires is_valid_request_code(request);
  @ assigns \nothing ;
  @ ensures \result == 0xff <==> (\forall integer i; 0 <= i < MAX_TRANSITION_STATE ==> usb_automaton[current_state].req_trans[i].request != request) ;
  @ ensures (\result != 0xff) <==> \exists integer i; 0 <= i < MAX_TRANSITION_STATE && usb_automaton[current_state].req_trans[i].request == request
            && \result == usb_automaton[current_state].req_trans[i].target_state ;
*/

uint8_t usbctrl_next_state(usb_device_state_t current_state,
                           usb_device_trans_t request)
{

  /*@
      @ loop invariant 0 <= i <= MAX_TRANSITION_STATE ;
      @ loop invariant \valid_read(usb_automaton[current_state].req_trans + (0..(MAX_TRANSITION_STATE -1)));
      @ loop invariant (\forall integer prei ; 0 <= prei < i ==> usb_automaton[current_state].req_trans[prei].request != request) ;
      @ loop assigns i ;
      @ loop variant MAX_TRANSITION_STATE -i ;
  */

    for (uint8_t i = 0; i < MAX_TRANSITION_STATE; ++i) {
        if (usb_automaton[current_state].req_trans[i].request == request) {
            return (usb_automaton[current_state].req_trans[i].target_state);
        }
    }
    /* fallback, no corresponding request found for  this state */
    return 0xff;
}

/*!
 * \brief Specify if the current request is valid for the current state
 *
 * \param current_state the current automaton state
 * \param request       the current transition request
 *
 * \return true if the transition request is allowed for this state, or false
 */

/*@
    @ requires is_valid_state(current_state) ;
    @ requires is_valid_transition(transition);
    @ requires \valid_read(usb_automaton[current_state].req_trans + (0..(MAX_TRANSITION_STATE -1)));
    @ requires \separated(usb_automaton[current_state].req_trans + (0..(MAX_TRANSITION_STATE -1)));
    @ assigns \nothing;
    @ ensures \result == \true || \result == \false ;
    @ ensures \result == \true <==> (\exists integer i ; 0 <= i < MAX_TRANSITION_STATE && usb_automaton[current_state].req_trans[i].request == transition) ;
    @ ensures \result == \false <==> (\forall integer i ; 0 <= i < MAX_TRANSITION_STATE ==> usb_automaton[current_state].req_trans[i].request != transition );
    @ ensures \result == \true && transition == USB_DEVICE_TRANS_RESET ==>
                       !(current_state == USB_DEVICE_STATE_ATTACHED) ;
    @ ensures \result == \false && transition == USB_DEVICE_TRANS_RESET ==>
        (current_state == USB_DEVICE_STATE_ATTACHED) ;
*/

bool usbctrl_is_valid_transition(usb_device_state_t current_state,
                                 usb_device_trans_t transition)
{
  /*@
      @ loop invariant 0 <= i <= MAX_TRANSITION_STATE ;
      @ loop invariant \valid_read(usb_automaton[current_state].req_trans + (0..(MAX_TRANSITION_STATE -1)));
      @ loop invariant (\forall integer prei ; 0 <= prei < i ==> usb_automaton[current_state].req_trans[prei].request != transition) ;
      @ loop assigns i ;
      @ loop variant MAX_TRANSITION_STATE -i ;
  */

    for (uint8_t i = 0; i < MAX_TRANSITION_STATE; ++i) {
        if (usb_automaton[current_state].req_trans[i].request == transition) {
            return true;
        }
    }
    /*
     * Didn't find any request associated to current state. This is not a
     * valid transition. We should stall the request.
     */

    log_printf("%s: invalid transition from state %d, request %d\n", __func__, current_state, transition);

    return false;
}
