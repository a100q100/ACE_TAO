/* -*- C++ -*- */
/**
 *  @file   ECG_UDP_Receiver.h
 *
 *  $Id$
 *
 *  @author Carlos O'Ryan (coryan@cs.wustl.edu)
 *  @author Marina Spivak (marina@atdesk.com)
 *
 * Based on previous work by Tim Harrison (harrison@cs.wustl.edu) and
 * other members of the DOC group. More details can be found in:
 *
 * http://doc.ece.uci.edu/~coryan/EC/index.html
 *
 * Define helper classes to propagate events between ECs using
 * either UDP or multicast.
 * The architecture is a bit complicated and deserves some
 * explanation: sending the events over UDP (or mcast) is easy, a
 * Consumer (TAO_ECG_UDP_Sender) subscribes for a certain set of
 * events, its push() method marshalls the event set into a CDR
 * stream that is sent using an ACE_SOCK_Dgram. The subscription
 * set and IP address can be configured.
 * Another helper class (TAO_ECG_UDP_Receiver) acts as a supplier of
 * events; it receives a callback when an event is available on an
 * ACE_SOCK_Dgram, it demarshalls the event and pushes it to the
 * EC.  Two ACE_Event_Handler classes are provided that can forward
 * the events to this Supplier: TAO_ECG_Mcast_EH can receive events
 * from a multicast group; TAO_ECG_UDP_EH can receive events from a
 * regular UDP socket.
 *
 * @todo The class makes an extra copy of the events, we need to
 * investigate if closer collaboration with its collocated EC could
 * be used to remove that copy.
 */

#ifndef TAO_ECG_UDP_RECEIVER_H
#define TAO_ECG_UDP_RECEIVER_H
#include "ace/pre.h"

#include "orbsvcs/RtecUDPAdminS.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "orbsvcs/RtecEventChannelAdminS.h"
#include "orbsvcs/Event/event_export.h"

#include "ECG_Adapters.h"
#include "EC_Lifetime_Utils.h"
#include "EC_Lifetime_Utils_T.h"
#include "ECG_CDR_Message_Receiver.h"

class TAO_ECG_UDP_Out_Endpoint;
class ACE_Reactor;

/**
 * @class TAO_ECG_UDP_Receiver_Disconnect_Command
 *
 * @brief Disconnects supplier represented by <proxy> from the Event Channel.
 *
 * Utility class for use as a template argument to TAO_EC_Auto_Command.
 * TAO_EC_Auto_Command<TAO_ECG_UDP_Receiver_Disconnect_Command> manages
 * supplier connection to the Event Channel, automatically disconnecting from
 * <proxy> in its destructor, if necessary.
 */
class TAO_RTEvent_Export TAO_ECG_UDP_Receiver_Disconnect_Command
{
public:
  TAO_ECG_UDP_Receiver_Disconnect_Command (void);
  TAO_ECG_UDP_Receiver_Disconnect_Command (
              RtecEventChannelAdmin::ProxyPushConsumer_ptr proxy);

  TAO_ECG_UDP_Receiver_Disconnect_Command (
              const TAO_ECG_UDP_Receiver_Disconnect_Command & rhs);

  TAO_ECG_UDP_Receiver_Disconnect_Command &
   operator= (const TAO_ECG_UDP_Receiver_Disconnect_Command & rhs);

  void execute (ACE_ENV_SINGLE_ARG_DECL);

private:

  RtecEventChannelAdmin::ProxyPushConsumer_var proxy_;
};

/**
 * @class TAO_ECG_UDP_Receiver
 *
 * @brief Receive events from UDP or Multicast and push them to a
 *        "local" EC.
 *        NOT THREAD-SAFE.
 *
 * This class connects as a supplier to an EventChannel, and supplies
 * to it all events it receives via UDP or Multicast.
 */
class TAO_RTEvent_Export TAO_ECG_UDP_Receiver
  : public virtual PortableServer::RefCountServantBase
  , public virtual POA_RtecEventComm::PushSupplier
  , public TAO_EC_Deactivated_Object
  , public TAO_ECG_Dgram_Handler
{
public:

  /// Initialization and termination methods.
  //@{

  /// Create a new TAO_ECG_UDP_Receiver object.
  /// (Constructor access is restricted to insure that all
  /// TAO_ECG_UDP_Receiver objects are heap-allocated.)
  static TAO_EC_Servant_Var<TAO_ECG_UDP_Receiver> create (CORBA::Boolean perform_crc = 0);

  ~TAO_ECG_UDP_Receiver (void);

  /**
   * @param lcl_ec Event Channel to which we will act as a supplier of events.
   * @param ignore_from Endpoint used to remove events generated by
   *        the same process.
   * @param add_server Address server used to obtain mapping of event type
   *        to multicast group.
   * To insure proper resource clean up, if init () is successful,
   * shutdown () must be called when the receiver is no longer needed.
   * This is done by disconnect_push_supplier() method.  If
   * disconnect_push_supplier() will not be called, it is the
   * responsibility of the user.
   * Furthermore, if shutdown() is not explicitly called by
   * either disconnect_push_supplier () or the user, the receiver
   * will clean up the resources in its destructor, however, certain
   * entities involved in cleanup must still exist at that point,
   * e.g., POA.
   */
  void init (RtecEventChannelAdmin::EventChannel_ptr lcl_ec,
             TAO_ECG_Refcounted_Endpoint ignore_from,
             RtecUDPAdmin::AddrServer_ptr addr_server
             ACE_ENV_ARG_DECL);

  /// Connect or reconnect to the EC with the given publications.
  /**
   * NOTE: if we are already connected to EC and a reconnection is
   * necessary, the EC must have reconnects enabled in order for this
   * method to succeed.
   */
  void connect (const RtecEventChannelAdmin::SupplierQOS& pub
                ACE_ENV_ARG_DECL);

  /// Set the handler we must notify when shutdown occurs.  (This is
  /// the handler that alerts us when data is available on udp/mcast socket.)
  /// Shutdown notification gives the handler an opportunity to properly clean
  /// up resources.
  void set_handler_shutdown (TAO_ECG_Refcounted_Handler handler_shutdown_rptr);

  /// Deactivate from POA and disconnect from EC, if necessary.  Shut
  /// down all receiver components.
  /**
   * If this class is used with refcounting, calling this method may
   * result in decrementing of the reference count (due to
   * deactivation) and deletion of the object.
   */
  void shutdown (ACE_ENV_SINGLE_ARG_DECL);
  //@}

  /// Accessor.
  /// Call the RtecUDPAdmin::AddrServer::get_addr.  Throws exception
  /// if nill Address Server was specified in init ().
  void get_addr (const RtecEventComm::EventHeader& header,
                 RtecUDPAdmin::UDP_Addr_out addr
                 ACE_ENV_ARG_DECL);

  /// The PushSupplier idl method.
  /// Invokes shutdown (), which may result in the object being deleted, if
  /// refcounting is used to manage its lifetime.
  virtual void disconnect_push_supplier (ACE_ENV_SINGLE_ARG_DECL)
      ACE_THROW_SPEC ((CORBA::SystemException));

  /// TAO_ECG_Dgram_Handler method.
  /**
   * UDP/Multicast Event_Handlers call this method when data is
   * available at the socket - the <dgram> is ready for reading.
   * Data is read from the socket, and, if complete message is
   * received, the event is pushed to the local Event Channel.
   */
  virtual int handle_input (ACE_SOCK_Dgram& dgram);

protected:

  /// Constructor (protected).  Clients can create new
  /// TAO_ECG_UDP_Receiver objects using the static create() method.
  TAO_ECG_UDP_Receiver (CORBA::Boolean perform_crc = 0);

private:

  /// Helpers for the connect() method.
  //@{
  // Establishes connection to the Event Channel for the first time.
  void new_connect (const RtecEventChannelAdmin::SupplierQOS& pub
                    ACE_ENV_ARG_DECL);

  // Updates existing connection to the Event Channel.
  void reconnect (const RtecEventChannelAdmin::SupplierQOS& pub
                  ACE_ENV_ARG_DECL);
  //@}

  /// Event Channel to which we act as a supplier.
  RtecEventChannelAdmin::EventChannel_var lcl_ec_;

  /// The server used to map event types to multicast groups.
  RtecUDPAdmin::AddrServer_var addr_server_;

  /// Proxy used to supply events to the Event Channel.
  RtecEventChannelAdmin::ProxyPushConsumer_var consumer_proxy_;

  /// Helper for reading incoming UDP/Multicast messages.  It assembles
  /// message fragments and provides access to a cdr stream once the
  /// complete message has been received.
  TAO_ECG_CDR_Message_Receiver cdr_receiver_;

  // Handler we must notify when shutdown occurs, so it has an
  // opportunity to clean up resources.
  TAO_ECG_Refcounted_Handler handler_rptr_;

  typedef TAO_EC_Auto_Command<TAO_ECG_UDP_Receiver_Disconnect_Command>
  ECG_Receiver_Auto_Proxy_Disconnect;
  /// Manages our connection to Consumer Proxy.
  ECG_Receiver_Auto_Proxy_Disconnect auto_proxy_disconnect_;
};


#if defined(__ACE_INLINE__)
#include "ECG_UDP_Receiver.i"
#endif /* __ACE_INLINE__ */

#include "ace/post.h"
#endif /* TAO_ECG_UDP_RECEIVER_H */
