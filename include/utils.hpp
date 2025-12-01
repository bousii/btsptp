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
	Bvars value;
};

enum Charcodes : unsigned char {
	B_INT = 'i',
	B_LIST = 'l',
	B_DICT = 'd',
	B_END = 'e',
	B_STR = ':',
};

/* Bencoded file parser, returns a Bdeptr to a tree of values */
class Bdecoder {

public:
    explicit Bdecoder(const std::vector<unsigned char>& data);
    Bdeptr decode();

private:
    Bdeptr parseValue();
    Bdeptr parseInt();
    Bdeptr parseString();
    Bdeptr parseList();
    Bdeptr parseDict();
	
    const std::vector<unsigned char>& data_;
    size_t pos_ = 0;
};

void Bprint(Bdeptr node, int indent);

#endif /* utils.hpp */
