#ifndef logging_h
#define logging_h

#define TRACE do {std::cout << "TRACE " __FILE__ ":" << __LINE__ << ") " << __PRETTY_FUNCTION__ << " " << this << std::endl; } while (0)

#endif

