#ifndef FIX_PARSER_H_
#define FIX_PARSER_H_

#include <absl/log/log.h>
#include <absl/status/statusor.h>
#include <absl/strings/ascii.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pugixml.hpp>
#include <ranges>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

// use pipe for readability, instead of the actual SOH char
#define SOH '|'
// paths suck but this works
#define FIX_SPEC_PATH "spec/FIX44.xml"

namespace fs = std::filesystem;

namespace fixparser {

// the FixParser class provides an interface to print a FIX message into something
// human readable from a std::string directly or from reading a file. it can also
// read multiple FIX messages from a file, printing them all.
class FixParser {
private:
    // the FIX message consists of n pairs delimited by a character. this is an
    // unparsed field, as its constructed from the raw FIX message before the
    // FIX spec is parsed.
    struct Field {
        // tags start at 1, so we can assume 0 means the tag hasn't been set yet
        int tag {};
        std::string value;

        friend std::ostream& operator<<(std::ostream& os, const Field& kvp)
        {
            os << "(" << kvp.tag << ", " << kvp.value << ")";
            return os;
        }
    };

    // some fields have values like
    // <value enum='N' description='NEW' />
    struct FixFieldValue {
        std::string enum_value;
        std::string description;
    };

    // field data structure for the FIX message. e.g.
    // <field number='8' name='BeginString' type='STRING' />
    // they represent a tag and are associated with a value. e.g.
    // 8=FIX.4.4, where 8 is the tag and FIX.4.4 is the value.
    struct FixField {
        int number {};
        std::string name;
        std::string type;
        std::string value;
        std::string enum_description;
    };

    // header of the FIX message
    struct FixHeader {
        std::vector<FixField> header;
    };

    // trailer of the FIX message
    struct FixTrailer {
        std::vector<FixField> trailer;
    };

    // body of the FIX message
    struct FixBody {
        std::vector<FixField> body;
    };

    // the FIX message itself, comprised of the header, body, and trailer
    struct FixMessage {
        FixHeader header;
        FixBody body;
        FixTrailer trailer;
        // the string representation of the message, so we can print it later
        std::string raw_message;
    };

    std::vector<Field> unparsed_fields;

    // parses a FIX message, as string, into a FixMessage structure containing
    // the header, body, and trailer.
    absl::StatusOr<FixMessage> parseFixMessage(std::string& msg)
    {
        // strip any trailing whitespace otherwise std::stoi crashes
        // https://github.com/abseil/abseil-cpp/blob/master/absl/strings/ascii.h#L199-L239
        absl::StripTrailingAsciiWhitespace(&msg);

        // parse the FIX message into a series of tag,value pairs
        for (const auto& x : msg | std::views::split(SOH)) {
            Field fix;

            for (const auto& y : x | std::views::split('=')) {
                std::string m_buf;
                std::ranges::copy(y, std::back_insert_iterator(m_buf));

                if (!m_buf.empty()) {
                    if (fix.tag == 0) {
                        fix.tag = std::stoi(m_buf);
                    } else {
                        fix.value = m_buf;
                    }
                }
            }

            unparsed_fields.push_back(fix);
        }

        pugi::xml_document fix_spec;
        pugi::xml_parse_result xml_result = fix_spec.load_file(FIX_SPEC_PATH);

        FixMessage message;
        FixHeader header;
        FixTrailer trailer;
        FixBody body;

        // parsing the unparsed fields into fields
        if (xml_result) {
            for (auto pair : unparsed_fields) {

                auto headers = fix_spec.child("fix").child("header");
                auto trailers = fix_spec.child("fix").child("trailer");
                auto fields = fix_spec.child("fix").child("fields");

                auto tag = pair.tag;
                FixField field;

                // find corresponding field for a given tag
                auto is_field = fields.find_node([&tag](auto& node) {
                    return std::strcmp(node.attribute("number").as_string(), std::to_string(tag).c_str()) == 0;
                });

                if (is_field) {
                    // since a field exists with a matching tag, store it
                    field.name = static_cast<std::string>(is_field.attribute("name").as_string());
                    field.number = is_field.attribute("number").as_int();
                    field.type = static_cast<std::string>(is_field.attribute("type").as_string());
                    field.value = pair.value;

                    auto find_enum_value = [&field](auto& node) {
                        return std::strcmp(node.attribute("enum").as_string(), field.value.c_str()) == 0;
                    };

                    // the field may have values
                    for (auto& value : is_field.children()) {
                        // find the corresponding enum value. this will only be triggered once since
                        // the field.value is basically indexing into the values
                        if (find_enum_value(value)) {
                            FixFieldValue v;
                            v.description = static_cast<std::string>(value.attribute("description").as_string());
                            field.enum_description = v.description;
                            break;
                        }
                    }

                    // check if its present in the headers
                    auto is_in_header = headers.find_node([&is_field](auto& node) {
                        return std::strcmp(node.attribute("name").as_string(), is_field.attribute("name").as_string()) == 0;
                    });

                    // check if its present in the trailers
                    auto is_in_trailer = trailers.find_node([&is_field](auto& node) {
                        return std::strcmp(node.attribute("name").as_string(), is_field.attribute("name").as_string()) == 0;
                    });

                    // if the field is present in the header, add it to the header
                    // else if its present in the trailer, add it to that
                    // else its in the body
                    if (is_in_header) {
                        header.header.emplace_back(field);
                    } else if (is_in_trailer) {
                        trailer.trailer.emplace_back(field);
                    } else {
                        body.body.emplace_back(field);
                    }
                }
            }

            message.header = std::move(header);
            message.body = std::move(body);
            message.trailer = std::move(trailer);
            message.raw_message = msg;

            // clear this in the case of parsing multiple messages sequentially
            unparsed_fields.clear();

        } else {
            // at this point, the XML parsing has failed for some reason. we definitely
            // should never get here. most likely is incorrect path, but we'll use a more
            // generic error here anyway
            return absl::FailedPreconditionError("parsing of XML FIX spec failed");
        }

        return message;
    };

    void printField(const FixField& field)
    {
        // if value exists, then we can display the enum description instead. easier to understand
        if (!field.enum_description.empty()) {
            std::cout << std::setw(5) << field.number << std::setw(20) << field.name << ": " << field.enum_description << std::endl;
        } else {
            std::cout << std::setw(5) << field.number << std::setw(20) << field.name << ": " << field.value << std::endl;
        }
    }

    std::vector<std::string> readFile(const std::filesystem::path& file_path)
    {
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(line);
            }
            return lines;
        } else {
            throw std::runtime_error("Failed to open file: " + file_path.string());
        }
    }

    // the FIX message as a string
    std::vector<std::string> fix_msgs_;

public:
    FixParser(const std::string& msg)
        : fix_msgs_ { msg } {};
    FixParser(const fs::path& file_path)
        : fix_msgs_(readFile(file_path)) {};

    // compare the computed checksum to the checksum in the message. if they do not match, print an error.
    // uses the vector of messages, so can also validate checksums for multiple messages read from a file.
    void validateChecksum()
    {
        for (std::string& fix_msg : fix_msgs_) {
            int calculated_checksum = 0;

            // checksum will always be the last field, so trim that since that isn't used to calculate
            // the checksum
            size_t penultimate_soh = fix_msg.find_last_of('|', fix_msg.find_last_of('|') - 1);
            std::string trimmed_msg = fix_msg.substr(0, penultimate_soh + 1);

            for (auto& c : trimmed_msg) {
                // the SOH is 1 but since we use a |, manually set the value
                if (c == '|') {
                    calculated_checksum += 1;
                } else {
                    calculated_checksum += static_cast<int>(c);
                }
            }

            calculated_checksum %= 256;

            // parse the string and get the checksum from there
            auto parsed = parseFixMessage(fix_msg);
            if (parsed.ok()) {
                auto parsed_msg = parsed.value();

                for (auto& field : parsed_msg.trailer.trailer) {
                    if (std::strcmp(field.name.c_str(), "CheckSum") == 0) {
                        if (std::stoi(field.value) != calculated_checksum) {
                            PLOG(ERROR) << "invalid checksum for message '" << fix_msg << "'";
                        }
                    }
                }
            }
        }
    }

    // main interface to print the FIX message in a human readable way
    void pprint()
    {
        for (std::string& fix_msg : fix_msgs_) {
            auto parsed = parseFixMessage(fix_msg);

            if (parsed.ok()) {
                auto parsed_msg = parsed.value();

                std::cout << "FIX message:" << std::endl;
                std::cout << parsed_msg.raw_message << "\n\n";

                // print header
                std::cout << "Header:" << std::endl;
                for (const auto& field : parsed_msg.header.header) {
                    printField(field);
                }
                std::cout << "\n";

                // print body
                std::cout << "Body:" << std::endl;
                for (const auto& field : parsed_msg.body.body) {
                    printField(field);
                }
                std::cout << "\n";

                // print trailer
                std::cout << "Trailer:" << std::endl;
                for (const auto& field : parsed_msg.trailer.trailer) {
                    printField(field);
                }
                std::cout << "\n";
            } else {
                PLOG(ERROR) << "unexpected error " << parsed.status();
            }
        }
    };
};

}; // namespace fixparser

#endif // FIX_PARSER_H_
