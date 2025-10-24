#ifndef UTILS_HPP
#define UTILS_HPP
#include <string>
#include <vector>
#include <variant>
#include <map>
#include <memory>

struct Bdecode;

using Bdeptr = std::shared_ptr<Bdecode>;

/* 
 * This struct holds a potential object that is decoded.
 * It can be a variety of types: Integers, Strings, Lists, or Dictionaries(Maps)
 */
struct Bdecode {
	using Bvars = std::variant<
		int64_t,
		std::string,
		/* If it's a list or a dictionary, it can be recursive */
		std::vector<Bdeptr>, 
		std::map<std::string, Bdeptr> 
	>;
};

/* Bencoded file parser, returns a Bdeptr to a tree of values */
class Bdecoder {

public:
    explicit Bdecoder(const std::vector<unsigned char>& data);
    std::shared_ptr<Bdecode> decode();

private:
    std::shared_ptr<Bdecode> parseValue();
    std::shared_ptr<Bdecode> parseInt();
    std::shared_ptr<Bdecode> parseString();
    std::shared_ptr<Bdecode> parseList();
    std::shared_ptr<Bdecode> parseDict();

    const std::vector<unsigned char>& data_;
    size_t pos_ = 0;
};

#endif /* utils.hpp */
