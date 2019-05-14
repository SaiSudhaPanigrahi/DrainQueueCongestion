#ifndef PROTO_ERROR_CODES_H_
#define PROTO_ERROR_CODES_H_
namespace dqc{
// These values must remain stable as they are uploaded to UMA histograms.
// To add a new error code, use the current value of QUIC_LAST_ERROR and
// increment QUIC_LAST_ERROR.
enum ProtoErrorCode{
  PROTO_NO_ERROR = 0,
    // The packet contained no payload.
  PROTO_MISSING_PAYLOAD = 48,
};
const char* ProtoErrorCodeToString(ProtoErrorCode error);
}
#endif
