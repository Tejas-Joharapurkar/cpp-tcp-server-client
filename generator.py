# parse kar yeh file:
# message Order {
#     double   price
#     uint32   quantity
#     char[8]  symbol
#     uint32   side
# }
import os
size_map = {
    "double": 8,
    "uint32": 4,
    "uint16": 2,
    "uint8":  1,
    "char":   1,
}

padding_map = {
    1 : {"type" : "char", "name" : "padding", "size" : 1},
    2 : {"type" : "char", "name" : "padding", "size" : 2},
    3 : {"type" : "char", "name" : "padding", "size" : 3},
    4 : {"type" : "char", "name" : "padding", "size" : 4},
    5 : {"type" : "char", "name" : "padding", "size" : 5},
    6 : {"type" : "char", "name" : "padding", "size" : 6},
    7 : {"type" : "char", "name" : "padding", "size" : 7},
}
# schema type → C++ type
type_map = {
    "double": "double",
    "uint32": "std::uint32_t",
    "uint16": "std::uint16_t",
    "uint8":  "std::uint8_t",
    "char":   "char",
}

def get_size(field):
    if field["type"] == "char":
        return int(field["size"])
    return size_map[field["type"]]

messages = {}
def parse(filename):
    with open(filename, 'r') as file:
        fields = []
        for line in file:
            words = line.strip().split()
            if(len(words) == 0):
                continue
            if(words[0] == "message"):
                fields.clear()
                name = words[1]
            elif(words[0] == '}'):
                messages[name] = calculate_layout(fields)
            elif(words[0].count('char')):
                fields.append({"type" : words[0][:4], "name" : words[1], "size" : words[0][5:-1]})
            else:
                fields.append({"type" : words[0], "name" : words[1]})
    printmap(messages)

def get_padding(padding):
    return padding_map[padding]

def calculate_layout(fields):
    result = []
    offset = 0

    # descending sort
    fields.sort(key=lambda x: get_size(x), reverse=True)

    for field in fields:
        size = get_size(field)

        # alignment — char ka 1, baaki apna size
        alignment = 1 if field["type"] == "char" else size

        # kya misaligned hai?
        remainder = offset % alignment
        if remainder != 0:
            padding = alignment - remainder
            result.append({"type": "char", "name": f"filler_{padding}", "size": str(padding)})
            offset += padding

        result.append(field)
        offset += size

    # end padding
    remainder = offset % 8
    if remainder != 0:
        padding = 8 - remainder
        result.append({"type": "char", "name": f"filler_{padding}", "size": str(padding)})

    return result
def printmap(messages):
        for message in messages.keys():
            print(message)
            for field in messages[message]:
                if(field["type"] == 'char'):
                    print(f"{field["type"]} {field["size"]} {field["name"]}")
                else:
                    print(f"{field["type"]} {field["name"]}")
    

def GenrateMessage(messages):
    os.makedirs("Messages", exist_ok=True)
    for message in messages.keys():
        file_path = os.path.join("Messages",message)
        with open(f"{file_path}.hxx","w") as file:
            file.write("#include <cstdint> \n")
            file.write(f"struct {message} {{ \n")
            for field in messages[message]:
                if(field["type"] == "char"):
                    file.write(f"char {field["name"]}[{field["size"]}];\n")
                else:
                    file.write(f"{type_map[field["type"]]} {field["name"]};\n")
            file.write("};\n")
            file.write(f"int encode(const {message}& msg, char* buf, int bufSize);\n")
            file.write(f"int decode(const char* buf, {message}& msg);\n")
            file.close()

        with open(f"{file_path}.cxx", "w") as file:
            file.write(f'#include "{message}.hxx"\n')
            file.write(f'#include <cstring>\n\n')
            
            # encode
            file.write(f"int encode(const {message}& msg, char* buf, int bufSize) {{\n")
            file.write(f"    char* ptr = buf;\n")
            for field in messages[message]:
                if field["type"] == "char":
                    file.write(f'    memcpy(ptr, msg.{field["name"]}, {field["size"]});\n ptr += {field["size"]};\n')
                else:
                    size = size_map[field["type"]]
                    file.write(f'    memcpy(ptr, &msg.{field["name"]}, {size});\n ptr += {size};\n')
            file.write(f"    return ptr - buf;\n")
            file.write(f"}}\n\n")
            
            # decode
            file.write(f"int decode(const char* buf, {message}& msg) {{\n")
            file.write(f"    const char* ptr = buf;\n")
            for field in messages[message]:
                if field["type"] == "char":
                    file.write(f'    memcpy(msg.{field["name"]}, ptr, {field["size"]});\n ptr += {field["size"]};\n')
                else:
                    size = size_map[field["type"]]
                    file.write(f'    memcpy(&msg.{field["name"]}, ptr, {size});\n ptr += {size};\n')
            file.write(f"    return ptr - buf;\n")
            file.write(f"}}\n")

parse('messages.msg')
GenrateMessage(messages)