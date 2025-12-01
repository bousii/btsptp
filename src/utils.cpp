#include <iostream>
#include <utils.hpp>
#include <vector>
#include <stdexcept>

Bdecoder::Bdecoder(const std::vector<unsigned char> &data)
	: data_(data)
{}

/* Returns pointer to root of the Bencode tree */
Bdeptr Bdecoder::decode()
{
	return parseValue();	
}

Bdeptr Bdecoder::parseValue()
{
	unsigned char c = data_[pos_];
	std::cout << c << std::endl;
	if (c == B_INT) return parseInt();
	else if (c == B_LIST) return parseList();
	else if (c == B_DICT) return parseDict();
	else if (std::isdigit(c)) return parseString();
	else throw std::runtime_error(
			"error parsing bencode token: unidentified token type");
}

Bdeptr Bdecoder::parseInt()
{
	pos_++;
	bool negative = data_[pos_] == '-';
	if (negative) {
		pos_++;
		if (!isdigit(data_[pos_])) {
			throw std::runtime_error(
					"error parsing bencode token: '-' not followed by digits");
		}
	}

	if (data_[pos_] == '0') {
		pos_++;
		if (isdigit(data_[pos_])) {
			throw std::runtime_error(
					"error parsing bencode token: invalid int, leading zeroes");
		}
	}
	
	int64_t value = 0;
	while (data_[pos_] != B_END) {
		if (pos_ >= data_.size()) {
			throw std::runtime_error(
					"error parsing bencode token: invalid int, missing digits");
		} else if (!isdigit(data_[pos_])) {
			throw std::runtime_error(
					"error parsing bencode token: invalid int");
		}
		/* Quick way of converting to int */
		value = value * 10 + (data_[pos_] - '0'); 
		pos_++;
	}

	pos_++; /* skip terminator */

	if (negative) {
		value = -value;
	}

	Bdeptr ret = std::make_shared<Bdecode>();
	ret->value = value;
	return ret;
}

Bdeptr Bdecoder::parseString()
{
	int len = 0;
	while (data_[pos_] != B_STR) {
		if (!isdigit(data_[pos_])) {
			std::cout << data_[pos_] << std::endl;
			throw std::runtime_error(
					"error parsing bencode token: invalid str, missing length");
		}
		len = len * 10 + (data_[pos_] - '0');
		pos_++;
	}

	pos_++; /* skip terminator */

	std::string value = "";
	while (len-- > 0) {
		if (pos_ >= data_.size()) {
			throw std::runtime_error(
					"error parsing bencode token: invalid str, length mismatch");
		}
		value += data_[pos_++];	
	}
	Bdeptr ret = std::make_shared<Bdecode>();
	ret->value = value;
	return ret;
}

Bdeptr Bdecoder::parseList()
{
	pos_++;
	Bdeptr list = std::make_shared<Bdecode>();	
	list->value = std::vector<Bdeptr>();
	auto &vec = std::get<std::vector<Bdeptr>>(list->value);	
	while (data_[pos_] != B_END) {
		if (pos_ >= data_.size()) {
			throw std::runtime_error(
					"error parsing bencode token: invalid list, len mismatch");
		}
		Bdeptr element = parseValue();
		vec.push_back(element);
	}

	pos_++; /* skip terminator */
	return list;
}

Bdeptr Bdecoder::parseDict()
{
	pos_++;
	Bdeptr dict = std::make_shared<Bdecode>();
	dict->value = std::map<std::string, Bdeptr>();
	auto &map = std::get<std::map<std::string, Bdeptr>>(dict->value);	
	while (data_[pos_] != B_END) {
		if (pos_ >= data_.size()) {
			throw std::runtime_error(
					"error parsing bencode token: invalid dict, len mismatch");
		}
		Bdeptr key = parseString();
		auto &str = std::get<std::string>(key->value);	
		Bdeptr val = parseValue();
		map.insert({str, val});	
	}

	pos_++; /* skip terminator */
	return dict;
}

void Bprint(Bdeptr node, int indent = 0) {
    if (!node) return; // safety check

    std::string ind(indent, ' ');

    std::visit([&](auto&& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, int64_t>) {
            std::cout << ind << value << "\n";
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << ind << "\"" << value << "\"\n";
        }
        else if constexpr (std::is_same_v<T, std::vector<Bdeptr>>) {
            std::cout << ind << "[\n";
            for (const auto& elem : value) {
                Bprint(elem, indent + 2);
            }
            std::cout << ind << "]\n";
        }
        else if constexpr (std::is_same_v<T, std::map<std::string, Bdeptr>>) {
            std::cout << ind << "{\n";
            for (const auto& [key, val] : value) {
                std::cout << ind << "  \"" << key << "\": ";
                if (std::holds_alternative<int64_t>(val->value) ||
                    std::holds_alternative<std::string>(val->value)) {
                    Bprint(val, 0); // inline for simple types
                } else {
                    std::cout << "\n";
                    Bprint(val, indent + 4); // nested for list/dict
                }
            }
            std::cout << ind << "}\n";
        }
    }, node->value);
}
