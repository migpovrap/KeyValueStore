#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

/// Opcodes for client-server communication.
/// These opcodes are used in a switch case to determine what to do with the
/// received message on the server.
/// These opcodes are also used by clients when sending messages to the server.
enum OperationCode {
  OP_CODE_CONNECT = 1,
  OP_CODE_DISCONNECT = 2,
  OP_CODE_SUBSCRIBE = 3,
  OP_CODE_UNSUBSCRIBE = 4,
};

#endif  // COMMON_PROTOCOL_H
