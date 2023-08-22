#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

template <typename T>
T swapEndianness(const T& value) {
    T result;
    const unsigned char* src = (const unsigned char*)(&value);
    unsigned char* dest = (unsigned char*)(&result);

    for (size_t i = 0; i < sizeof(T); ++i) {
        dest[i] = src[sizeof(T) - i - 1];
    }

    return result;
}

struct JavaClassHeader1 {
	uint32_t magic;
	uint16_t minor;
	uint16_t major;
	uint16_t constantPoolCount;
} __attribute__((packed));

// Here is Constant pool...

struct JavaClassHeader2 {
	uint16_t access_flags;
	uint16_t this_class;
	uint16_t super_class;
	uint16_t interfaces_count;
} __attribute__((packed));

enum ConstantPoolTypes : uint8_t {
	Utf8 = 1,
	Class =	7,
	Fieldref = 9,
	Methodref = 10,
	InterfaceMethodref = 11,
	String = 8,
	Integer = 3,
	Float = 4,
	Long = 5,
	Double = 6,
	NameAndType = 12,
	MethodHandle = 15,
	MethodType = 16,
	InvokeDynamic = 18
};

struct ConstantPoolEntry {
	uint8_t tag;

	union {
		struct {
			uint16_t name_index;
		} Class;
		
		struct {
			uint16_t class_index;
			uint16_t name_and_type_index;
		} Reference;
		
		struct {
			uint16_t string_index;
		} String;

		struct {
			uint32_t number_bytes;
		} Numeric;

		struct {
			uint32_t low_number_bytes;
			uint32_t high_number_bytes;
		} LongNumeric;
		
		struct {
			uint16_t name_idx;
			uint16_t descriptor_idx;
		} NameAndType;

		struct {
			uint16_t length;
			string* bytes;
		} Utf8;
	};

	void read(ifstream& stream) {
		stream.read((char*)&tag, 1);  // Read tag

		if(tag == ConstantPoolTypes::Methodref) {
			stream.read((char*)&Reference, sizeof Reference);  // Read data
			
			Reference.class_index = swapEndianness(Reference.class_index);
			Reference.name_and_type_index = swapEndianness(Reference.name_and_type_index);
		} else if(tag == ConstantPoolTypes::Class) {
			stream.read((char*)&Class, sizeof Class);

			Class.name_index = swapEndianness(Class.name_index);
		} else if(tag == ConstantPoolTypes::NameAndType) {
			stream.read((char*)&NameAndType, sizeof NameAndType);

			NameAndType.name_idx = swapEndianness(NameAndType.name_idx);
			NameAndType.descriptor_idx = swapEndianness(NameAndType.descriptor_idx);
		} else if(tag == ConstantPoolTypes::Utf8) {
			stream.read((char*)&(Utf8.length), 2);

			Utf8.length = swapEndianness(Utf8.length);

			Utf8.bytes = new string("");

			char tmp;
			for(auto i = 0; i < Utf8.length; i++) {
				stream.read((char*)&tmp, 1);

				*Utf8.bytes += tmp;
			}
		} else if(tag == ConstantPoolTypes::Fieldref) {
			stream.read((char*)&Reference, sizeof Reference);

			Reference.class_index = swapEndianness(Reference.class_index);
			Reference.name_and_type_index = swapEndianness(Reference.name_and_type_index);
		} else if(tag == ConstantPoolTypes::String) {
			stream.read((char*)&String, sizeof String);

			String.string_index = swapEndianness(String.string_index);
		} else {
			cerr << "Error: Unknown constant pool element with tag: " << (uint32_t)tag << endl;
			exit(1);
		}
	}

	friend ostream& operator << (ostream& s, struct ConstantPoolEntry& info) {
		s << "<ConstantPoolEntry = TAG: " << (uint32_t)info.tag << "; ";

		if(info.tag == ConstantPoolTypes::Fieldref) {
			s << "Field reference";
		}else if(info.tag == ConstantPoolTypes::Methodref) {
			s << "Method reference";
		}else if(info.tag == ConstantPoolTypes::String) {
			s << "String@" << info.String.string_index - 1;
		}else if(info.tag == ConstantPoolTypes::Class) {
			s << "Class";
		}else if(info.tag == ConstantPoolTypes::NameAndType) {
			s << "Name and type";
		}else if(info.tag == ConstantPoolTypes::Utf8) {
			s << "Utf8; Value = \"" << *info.Utf8.bytes << "\"";
		}

		return s << ">";
	}

	~ConstantPoolEntry() {
		if(tag == ConstantPoolTypes::String) {
			delete Utf8.bytes;
		}
	}
};

class ConstantPool {
	public:
		ConstantPool(ifstream* stream, JavaClassHeader1* header) {
			this->stream = stream;
			this->header1 = header;

			entries = new ConstantPoolEntry[header->constantPoolCount - 1];

			for(auto i = 0; i < header->constantPoolCount - 1; i++) {
				entries[i].read(*stream);

				// cout << entries[i] << endl;
			}
		}

		std::string* get_name_by_index(uint32_t idx) {
			return entries[idx].Utf8.bytes;
		}

		~ConstantPool() {
			if(entries) {
				delete entries;
			}
		}

		ConstantPoolEntry* entries = nullptr;

	private:
		ifstream* stream;
		JavaClassHeader1* header1;
};

struct AttributeInfo {
	uint16_t attribute_name_index;
	uint32_t attribute_length;
	uint8_t* info;

	void read(ifstream& stream) {
		stream.read((char*)&attribute_name_index, 2);
		stream.read((char*)&attribute_length, 4);

		attribute_name_index = swapEndianness(attribute_name_index);
		attribute_length = swapEndianness(attribute_length);
		
		info = new uint8_t[attribute_length];

		stream.read((char*)info, attribute_length);
	}

	~AttributeInfo() {
		delete[] info;
	}

	friend ostream& operator << (ostream& s, struct AttributeInfo& info) {
		return s << "====\n" << "NAME INDEX: " << info.attribute_name_index <<
		 endl << "LEN: " << info.attribute_length <<
		 endl << "DATA: @" << (void*)info.info;
	}
};

struct FieldEntry {
	uint16_t access_flags;
	uint16_t name_index;
	uint16_t descriptor_index;
	uint16_t attributes_count;
	AttributeInfo* attributes;

	void read(ifstream& stream) {
		stream.read((char*)&access_flags, 2);
		stream.read((char*)&name_index, 2);
		stream.read((char*)&descriptor_index, 2);
		stream.read((char*)&attributes_count, 2);

		access_flags = swapEndianness(access_flags);
		name_index = swapEndianness(name_index);
		descriptor_index = swapEndianness(descriptor_index);
		attributes_count = swapEndianness(attributes_count);

		attributes = new AttributeInfo[attributes_count];

		for(auto i = 0; i < attributes_count; i++) {
			attributes[i].read(stream);
		}
	}

	~FieldEntry() {
		delete[] attributes;
	}
}; 

struct MethodEntry {
	uint16_t access_flags;
	uint16_t name_index;
	uint16_t descriptor_index;
	uint16_t attributes_count;
	AttributeInfo* attributes;

	void read(ifstream& stream) {
		stream.read((char*)&access_flags, 2);
		stream.read((char*)&name_index, 2);
		stream.read((char*)&descriptor_index, 2);
		stream.read((char*)&attributes_count, 2);

		access_flags = swapEndianness(access_flags);
		name_index = swapEndianness(name_index);
		descriptor_index = swapEndianness(descriptor_index);
		attributes_count = swapEndianness(attributes_count);

		attributes = new AttributeInfo[attributes_count];

		for(auto i = 0; i < attributes_count; i++) {
			attributes[i].read(stream);
		}
	}

	AttributeInfo* get_attribute(ConstantPool* pool, std::string name) {
		for(int i = 0; i < attributes_count; i++) {
			string* aname = pool->entries[attributes[i].attribute_name_index - 1].Utf8.bytes;

			if(*aname == name) {
				return &(attributes[i]);
			}
		}

		return nullptr;
	}

	friend ostream& operator << (ostream& s, struct MethodEntry& info) {
		s << "<Method = AccessFlags: " << info.access_flags << "; NameIndex: ";
		s << info.name_index << "; DescriptorIndex: " << info.descriptor_index;
		s << "; AttributesCount: " << info.attributes_count << ">";

		return s;
	}

	~MethodEntry() {
		delete[] attributes;
	}
}; 

class JavaClassFile {
	public:
	JavaClassFile(string filename) {
		stream = new ifstream(filename, ios::in | ios::binary);

		if (!stream) {
			cerr << filename << ": Failed to open the file." << endl;
			return;
		}

		stream->read((char*)&header1, sizeof header1);

		header1.magic = swapEndianness(header1.magic);
		header1.minor = swapEndianness(header1.minor);
		header1.major = swapEndianness(header1.major);
		header1.constantPoolCount = swapEndianness(header1.constantPoolCount);

		if(header1.magic != 0xCAFEBABE) {
			cout << filename << ": not a Java Class file!" << endl;
			return;
		}

		const_pool = new ConstantPool(stream, &header1);

		stream->read((char*)&header2, sizeof header2);

		header2.access_flags = swapEndianness(header2.access_flags);
		header2.this_class = swapEndianness(header2.this_class);
		header2.super_class = swapEndianness(header2.super_class);
		header2.interfaces_count = swapEndianness(header2.interfaces_count);

		if(header2.interfaces_count) {
			interfaces = new uint16_t[header2.interfaces_count];

			for(int i = 0; i < header2.interfaces_count; i++) {
				stream->read((char*)&(interfaces[i]), 2);
			}
		}

		stream->read((char*)&fields_count, 2);
		fields_count = swapEndianness(fields_count);

		fields = new FieldEntry[fields_count];
		for(auto i = 0; i < fields_count; i++) {
			fields[i].read(*stream);
		}
		
		stream->read((char*)&methods_count, 2);
		methods_count = swapEndianness(methods_count);
		
		methods = new MethodEntry[methods_count];
		for(auto i = 0; i < methods_count; i++) {
			methods[i].read(*stream);
		}

		stream->read((char*)&attributes_count, 2);
		attributes_count = swapEndianness(attributes_count);

		attributes = new AttributeInfo[attributes_count];
		for(auto i = 0; i < attributes_count; i++) {
			attributes[i].read(*stream);
		}

		ok = true;
	}

	MethodEntry* get_method(std::string name) {
		for(int i = 0; i < methods_count; i++) {
			string* mname = const_pool->entries[methods[i].name_index - 1].Utf8.bytes;

			if(*mname == name) {
				return &(methods[i]);
			}
		}

		return nullptr;
	}

	~JavaClassFile() {
		delete stream;
		delete const_pool;

		if(header2.interfaces_count) {
			delete interfaces;
		}

		delete fields;
		delete methods;
		delete attributes;
	}

	bool ok = false;
	string filename;
	struct JavaClassHeader1 header1;
	struct JavaClassHeader2 header2;
	
	ConstantPool* const_pool = nullptr;
	uint16_t* interfaces = nullptr;
	uint16_t fields_count = 0;
	
	FieldEntry* fields = nullptr;
	uint16_t methods_count = 0;
	
	MethodEntry* methods = nullptr;
	
	uint16_t attributes_count = 0;
	AttributeInfo* attributes = nullptr;

	private:
	ifstream* stream = nullptr;
};

struct CodeAttribute {
	uint16_t max_stack;
	uint16_t max_locals;
	uint32_t code_length;
} __attribute__((packed));

enum JavaOpCodes : uint8_t {
	GETSTATIC = 0xB2,
	SIPUSH    = 0x11,
	LDC       = 0x12,
	ILOAD_1   = 0x1b,
	INVOKEVIRTUAL = 0xB6,
	INVOKESTATIC = 0xB8,
	RETURN    = 0xB1
};

enum ActionID {
	STATIC = 0,
	CONSTANT
};

struct Action {
	enum ActionID id;
	virtual ~Action() = default;
};

struct StaticAction : public Action {
	string class_name;
	string name;
};

struct ConstantAction : public Action {
	ConstantPoolEntry entry;
};

struct ValueAction : public Action {
	int entry;
};

class JavaExecutor {
	public:
	JavaExecutor(JavaClassFile* klass) {
		this->klass = klass;

		/*
		MethodEntry* mainmethod = klass->get_method("main");
		
		codeattr_raw = mainmethod->get_attribute(klass->const_pool, "Code");
		codeattr = (CodeAttribute*)codeattr_raw->info;

		codeattr->max_stack = swapEndianness(codeattr->max_stack);
		codeattr->max_locals = swapEndianness(codeattr->max_locals);
		codeattr->code_length = swapEndianness(codeattr->code_length);

		codeptr = (uint8_t*)(codeattr_raw->info + sizeof(struct CodeAttribute));
		*/
		// codeptr = get_code("main");
	}

	MethodEntry* get_method(string method_name) {
		return klass->get_method(method_name);
	}

	struct CodeAttribute* get_code_attribute(MethodEntry* entry, string method_name) {
		if(!entry) {
			cout << "Exception: Trying to get Code Attribute from nullptr method!" << endl;
			cout << "           Method name: " << method_name << endl;
			exit(1);
		}
		
		AttributeInfo* codeattr_raw = entry->get_attribute(klass->const_pool, "Code");
		struct CodeAttribute* codeattr = (CodeAttribute*)codeattr_raw->info;

		codeattr->max_stack = swapEndianness(codeattr->max_stack);
		codeattr->max_locals = swapEndianness(codeattr->max_locals);
		codeattr->code_length = swapEndianness(codeattr->code_length);

		return codeattr;
	}

	uint8_t* get_code(MethodEntry* method) {
		return (uint8_t*)(method->get_attribute(klass->const_pool, "Code")->info + sizeof(struct CodeAttribute));
	}
	
	~JavaExecutor() {}

	void step(uint8_t* codeptr, size_t& cursor) {
		uint8_t op = codeptr[cursor];

		if(op == GETSTATIC) {
			// cout << "GetStatic!" << endl;

			cursor++;

			uint16_t idx = *((uint16_t*)(codeptr + cursor));

			idx = swapEndianness(idx);

			cursor++;

			ConstantPoolEntry entry = klass->const_pool->entries[idx - 1];

			ConstantPoolEntry class_index = klass->const_pool->entries[entry.Reference.class_index - 1];
			ConstantPoolEntry classname_index = klass->const_pool->entries[class_index.Class.name_index - 1];
			
			ConstantPoolEntry name_type_index = klass->const_pool->entries[entry.Reference.name_and_type_index - 1];
			ConstantPoolEntry name = klass->const_pool->entries[name_type_index.NameAndType.name_idx - 1];
			ConstantPoolEntry descriptor = klass->const_pool->entries[name_type_index.NameAndType.descriptor_idx - 1];
			
			// cout << "Class: " << entry << endl;
			// cout << "Class index: " << class_index << endl;
			// cout << "Class name: " << classname_index << endl;
			// cout << "Name and type: " << name_type_index << endl;
			// cout << "|- Name: " << name << endl;
			// cout << "|- Descriptor: " << descriptor << endl;

			StaticAction* action = new StaticAction;
			action->id = STATIC;
			action->class_name = *classname_index.Utf8.bytes;
			action->name = *name.Utf8.bytes;

			action_stack.push(action);
		} else if(op == LDC) {
			// cout << "LDC!" << endl;
			
			cursor++;

			uint8_t idx = codeptr[cursor];

			ConstantPoolEntry entry = klass->const_pool->entries[idx - 1];

			// cout << entry << endl;

			ConstantAction* action = new ConstantAction;
			action->id = CONSTANT;
			action->entry = entry;

			action_stack.push(action);
		} else if(op == INVOKEVIRTUAL) {
			// cout << "Invoke virtual!" << endl;

			cursor++;

			uint16_t idx = *((uint16_t*)(codeptr + cursor));

			idx = swapEndianness(idx);

			cursor++;

			ConstantPoolEntry entry = klass->const_pool->entries[idx - 1];

			ConstantPoolEntry class_index = klass->const_pool->entries[entry.Reference.class_index - 1];
			ConstantPoolEntry classname_index = klass->const_pool->entries[class_index.Class.name_index - 1];
			
			ConstantPoolEntry name_type_index = klass->const_pool->entries[entry.Reference.name_and_type_index - 1];
			ConstantPoolEntry name = klass->const_pool->entries[name_type_index.NameAndType.name_idx - 1];
			ConstantPoolEntry descriptor = klass->const_pool->entries[name_type_index.NameAndType.descriptor_idx - 1];
			
			// cout << "Class: " << entry << endl;
			// cout << "Class index: " << class_index << endl;
			// cout << "Class name: " << classname_index << endl;
			// cout << "Name and type: " << name_type_index << endl;
			// cout << "|- Name: " << name << endl;
			// cout << "|- Descriptor: " << descriptor << endl;

			if(*classname_index.Utf8.bytes == "java/io/PrintStream"
			   && *name.Utf8.bytes == "println") {
			   	ConstantAction* action = (ConstantAction*)action_stack.top();

			   	// TODO: Check types like int, float, string and so on...

				// cout << "Debug: " << action->entry << endl;
			   	cout << *klass->const_pool->entries[action->entry.String.string_index - 1].Utf8.bytes << endl;

			   	action_stack.pop();
			}
		} else if (op == INVOKESTATIC) {
			cout << "Invoke static!" << endl;

			cursor++;

			uint16_t idx = *((uint16_t*)(codeptr + cursor));

			idx = swapEndianness(idx);

			cursor++;

			ConstantPoolEntry entry = klass->const_pool->entries[idx - 1];

			ConstantPoolEntry class_index = klass->const_pool->entries[entry.Reference.class_index - 1];
			ConstantPoolEntry classname_index = klass->const_pool->entries[class_index.Class.name_index - 1];
			
			ConstantPoolEntry name_type_index = klass->const_pool->entries[entry.Reference.name_and_type_index - 1];
			ConstantPoolEntry name = klass->const_pool->entries[name_type_index.NameAndType.name_idx - 1];
			ConstantPoolEntry descriptor = klass->const_pool->entries[name_type_index.NameAndType.descriptor_idx - 1];

			cout << "Class: " << entry << endl;
			cout << "Class index: " << class_index << endl;
			cout << "Class name: " << classname_index << endl;
			cout << "Name and type: " << name_type_index << endl;
			cout << "|- Name: " << name << endl;
			cout << "|- Descriptor: " << descriptor << endl;

			run(*name.Utf8.bytes);
		} else if (op == SIPUSH) {
			cursor++;

			uint8_t sh1 = codeptr[cursor];

			cursor++;
			
			uint8_t sh2 = codeptr[cursor];

			int num = (int)((sh1 << 8) | sh2);

			cout << "Number is: " << num << endl;

			ValueAction* value = new ValueAction;
			value->id = CONSTANT;
			value->entry = num;

			action_stack.push(value);
		} else if (op == ILOAD_1) {
			cout << "Iload 1!" << endl;

			cout << "Need a local variable array!" << endl;

			exit(1);
		} else if (op == RETURN) {
			cout << "Return!" << endl;
		} else {
			cout << "Unknown instruction: 0x" << hex << (uint32_t)op << endl;
			exit(1);
		}
		
		cursor++;
	}

	void run(string method_name) {
		size_t cursor = 0;

		MethodEntry* method = get_method(method_name);
		struct CodeAttribute* codeattr = get_code_attribute(method, method_name);

		cout << "[" << method_name << "] " << "Length of LVA: " << codeattr->max_locals << endl;
		
		uint8_t* code = get_code(method);
		
		while(cursor < codeattr->code_length) {
			step(code, cursor);
		}
	}

	JavaClassFile* klass = nullptr;

	stack<struct Action*> action_stack;
};

int main(int argc, char** argv) {
	if(argc < 2) {
		std::cout << "Too few arguments!" << std::endl;
		return 1;
	}

	string filename = string(argv[1]);

	JavaClassFile file(filename);
	JavaExecutor executor(&file);

	executor.run("main");
	
	return 0;
}
