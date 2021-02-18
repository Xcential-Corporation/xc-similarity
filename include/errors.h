#ifndef ERRORS_H
#define ERRORS_H
#include <string>
#include <map>
#include <exception>
#include <vector>
#include <memory>

enum class ErrorKind {
    Parsing,
    Runtime,
    Citation,
    XML
};

extern const std::map<ErrorKind, std::string> errorKindString;

enum class ErrorType {
    // AMPL Runtime errors
    StringNotFound,
    ElementNotFound,
    RangeNotFound,
    PositionNotFound,
    InsertError,
    RenameError,
    RewriteError,
    Unimplemented,
    BadXML,
    // Parse Error
    UndefinedFunction,
    UnexpectedToken,
    AMPLNLError,
    CIMPLNLError,
    // Citation Error
    IdentifierNotFound,
    
    // Other
    Other
};

extern const std::map<ErrorType, std::string> errorTypeString;

enum class UserMessageType {
    ActNotFound,
    ProvisionNotFound,
    RangeNotFound,
    LikelySoftwareIssue,
    LikelyUserIssue,
    MissingLocation,
    CiteParseIssue,
    AmendmentParseIssue
};

extern const std::map<UserMessageType, std::string> userMessageTypeString;

enum class Severity {
    Error,
    Warning
};

extern const std::map<Severity, std::string> severityString;

class ErrorMsg
{
public:
    ErrorMsg(ErrorKind kind_, ErrorType type_, Severity severity_, UserMessageType umt, const std::string& msg, const std::string& extra_ = {});
    
    std::string msg() const {
        return errorKindString.at(kind)+" "+severityString.at(severity)+": "+errorTypeString.at(type)+". "+message;
    }
    
    ErrorKind kind;
    ErrorType type;
    Severity severity;
    UserMessageType userMT;
    std::string message;
    std::string extra;
    std::string amendment_id;
    std::vector<std::string> identifiers_ampl;
    int error_id;
};

typedef std::shared_ptr<ErrorMsg> ErrorMsgP;

class AMPLException: public ErrorMsg, public std::exception
{
public:
    AMPLException(ErrorKind kind_, ErrorType type_, Severity severity_, UserMessageType umt, const std::string& msg, const std::string& extra_ = {}):
        ErrorMsg(kind_, type_, severity_, umt, msg, extra_) {}

    virtual const char* what() const noexcept {
        static std::string res;
        res = msg();
        return res.c_str();
    }
};

#endif //ERRORS_H
