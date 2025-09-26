#pragma once
#include <cstring>
#include <utility>
#include "CoreMemory.h"
#include "CoreTypes.h"
#include "Containers/TArray.h"

namespace Rebel::Core {

class String {
public:
    // -------------------
    // Constructors
    // -------------------
    String() { sso[0] = '\0'; }

    String(const char* cstr) {
        MemSize len = std::strlen(cstr);
        if (len < SSO_SIZE) {
            std::memcpy(sso, cstr, len + 1);
            m_size = len;
            m_capacity = SSO_SIZE;
        } else {
            m_capacity = len + 1;
            heap = static_cast<char*>(Memory::DefaultAllocator().Allocate(m_capacity));
            std::memcpy(heap, cstr, len + 1);
            m_size = len;
        }
    }

    String(const String& other) {
        if (other.isSSO()) {
            std::memcpy(sso, other.sso, other.m_size + 1);
            m_size = other.m_size;
            m_capacity = SSO_SIZE;
        } else {
            m_capacity = other.m_size + 1;
            heap = static_cast<char*>(Memory::DefaultAllocator().Allocate(m_capacity));
            std::memcpy(heap, other.heap, other.m_size + 1);
            m_size = other.m_size;
        }
    }

    String(String&& other) noexcept {
        if (other.isSSO()) {
            std::memcpy(sso, other.sso, other.m_size + 1);
        } else {
            heap = other.heap;
            other.heap = nullptr;
        }
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_size = 0;
        other.m_capacity = SSO_SIZE;
    }

    ~String() {
        if (!isSSO() && heap) {
            Memory::DefaultAllocator().Free(heap, m_capacity);
        }
    }

    // -------------------
    // Assignment Operators
    // -------------------
    String& operator=(const String& other) {
        if (this == &other) return *this;

        if (!isSSO() && heap) Memory::DefaultAllocator().Free(heap, m_capacity);

        if (other.isSSO()) {
            std::memcpy(sso, other.sso, other.m_size + 1);
            m_capacity = SSO_SIZE;
        } else {
            m_capacity = other.m_size + 1;
            heap = static_cast<char*>(Memory::DefaultAllocator().Allocate(m_capacity));
            std::memcpy(heap, other.heap, other.m_size + 1);
        }
        m_size = other.m_size;
        return *this;
    }

    String& operator=(String&& other) noexcept {
        if (this == &other) return *this;

        if (!isSSO() && heap) Memory::DefaultAllocator().Free(heap, m_capacity);

        if (other.isSSO()) {
            std::memcpy(sso, other.sso, other.m_size + 1);
            m_capacity = SSO_SIZE;
        } else {
            heap = other.heap;
            m_capacity = other.m_capacity;
            other.heap = nullptr;
        }

        m_size = other.m_size;
        other.m_size = 0;
        other.m_capacity = SSO_SIZE;
        return *this;
    }
    // ================= Operator += =================
    String& operator+=(const String& other) {
        MemSize newSize = m_size + other.m_size;

        if (newSize <= 15) {
            // Both fit in SSO
            for (MemSize i = 0; i < other.m_size; ++i) {
                sso[m_size + i] = other[i];
            }
            sso[newSize] = '\0';
        } else {
            // Need heap allocation
            char* newHeap = new char[newSize + 1];

            // Copy existing string
            for (MemSize i = 0; i < m_size; ++i)
                newHeap[i] = isSSO() ? sso[i] : heap[i];

            // Copy other string
            for (MemSize i = 0; i < other.m_size; ++i)
                newHeap[m_size + i] = other[i];

            newHeap[newSize] = '\0';

            // Free old heap if needed
            if (!isSSO() && heap)
                delete[] heap;

            heap = newHeap;
        }

        m_size = newSize;
        return *this;
    }

    // Optional operator+ for convenience
    friend String operator+(String lhs, const String& rhs) {
        lhs += rhs;
        return lhs;
    }

    // -------------------
    // Append
    // -------------------
    void append(const char* cstr) {
        MemSize add_len = std::strlen(cstr);
        MemSize new_len = m_size + add_len;

        if (new_len < SSO_SIZE) {
            std::memcpy(sso + m_size, cstr, add_len + 1);
            m_size = new_len;
            return;
        }

        if (isSSO()) {
            // move SSO content to heap
            MemSize new_cap = new_len * 2;
            char* new_heap = static_cast<char*>(Memory::DefaultAllocator().Allocate(new_cap));
            std::memcpy(new_heap, sso, m_size);
            std::memcpy(new_heap + m_size, cstr, add_len + 1);
            heap = new_heap;
            m_capacity = new_cap;
        } else {
            if (new_len + 1 > m_capacity) {
                MemSize new_cap = new_len * 2;
                char* new_heap = static_cast<char*>(Memory::DefaultAllocator().Allocate(new_cap));
                std::memcpy(new_heap, heap, m_size);
                Memory::DefaultAllocator().Free(heap, m_capacity);
                heap = new_heap;
                m_capacity = new_cap;
            }
            std::memcpy(heap + m_size, cstr, add_len + 1);
        }
        m_size = new_len;
    }

    void append(const String& other) {
        append(other.c_str());
    }

    // -------------------
    // Operators
    // -------------------
    bool operator==(const String& other) const {
        return std::strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const String& other) const {
        return !(*this == other);
    }

    // -------------------
    // Utilities
    // -------------------
    const char* c_str() const { return isSSO() ? sso : heap; }
    MemSize length() const { return m_size; }

    uint32_t hash() const {
        const char* str = c_str();
        uint32_t hash = 2166136261u; // FNV-1a
        for (MemSize i = 0; i < m_size; i++) {
            hash ^= (uint32_t)str[i];
            hash *= 16777619u;
        }
        return hash;
    }
    char& operator[](MemSize index) { return isSSO() ? sso[index] : heap[index]; }
    const char& operator[](MemSize index) const { return isSSO() ? sso[index] : heap[index]; }

    // ================== Substr ==================
    String Substr(MemSize start, MemSize count) const {
        if (start >= m_size) return String();
        if (start + count > m_size) count = m_size - start;

        String result;
        if (count <= 15) {
            for (MemSize i = 0; i < count; ++i) {
                result.sso[i] = (*this)[start + i];
            }
            result.sso[count] = '\0';
        } else {
            result.heap = new char[count + 1];
            for (MemSize i = 0; i < count; ++i) {
                result.heap[i] = (*this)[start + i];
            }
            result.heap[count] = '\0';
        }
        result.m_size = count;
        return result;
    }

private:
    static constexpr MemSize SSO_SIZE = 16;
    union {
        char sso[SSO_SIZE];
        char* heap;
    };
    MemSize m_size = 0;
    MemSize m_capacity = SSO_SIZE;

    bool isSSO() const { return m_size < SSO_SIZE; }
};

    // ================= Trim =================
    inline String Trim(const String& str) {
        MemSize start = 0;
        MemSize end = str.length();

        while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) ++start;
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) --end;

        return str.Substr(start, end - start);
    }

    // ================= ToLower / ToUpper =================
    inline String ToLower(const String& str) {
        String result = str;
        for (MemSize i = 0; i < result.length(); ++i)
            result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
        return result;
    }

    inline String ToUpper(const String& str) {
        String result = str;
        for (MemSize i = 0; i < result.length(); ++i)
            result[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[i])));
        return result;
    }

    // ================= Split =================
    inline Memory::TArray<String> Split(const String& str, char delimiter) {
        Memory::TArray<String> tokens;
        MemSize start = 0;
        for (MemSize i = 0; i < str.length(); ++i) {
            if (str[i] == delimiter) {
                tokens.Add(str.Substr(start, i - start));
                start = i + 1;
            }
        }
        tokens.Add(str.Substr(start, str.length() - start));
        return tokens;
    }

    // ================= Join =================
    inline String Join(const Memory::TArray<String>& strings, const String& delimiter) {
        if (strings.Num() == 0) return String();
        String result = strings[0];
        for (MemSize i = 1; i < strings.Num(); ++i) {
            result += delimiter;
            result += strings[i];
        }
        return result;
    }

    // ================= Hashing =================
    // Optional precomputed hash for faster TMap / TSet lookup
    inline MemSize HashCaseInsensitive(const String& str) {
        MemSize hash = 0xcbf29ce484222325;
        for (MemSize i = 0; i < str.length(); ++i) {
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(str[i])));
            hash ^= static_cast<MemSize>(c);
            hash *= 0x100000001b3;
        }
        return hash;
    }

    // ================= StartsWith / EndsWith =================
    inline bool StartsWith(const String& str, const String& prefix) {
        if (prefix.length() > str.length()) return false;
        for (MemSize i = 0; i < prefix.length(); ++i)
            if (str[i] != prefix[i]) return false;
        return true;
    }

    inline bool EndsWith(const String& str, const String& suffix) {
        if (suffix.length() > str.length()) return false;
        MemSize offset = str.length() - suffix.length();
        for (MemSize i = 0; i < suffix.length(); ++i)
            if (str[i + offset] != suffix[i]) return false;
        return true;
    }

    // ================= UTF-8 Helpers =================
    // Basic: count number of codepoints in UTF-8 string
    inline MemSize UTF8Length(const String& str) {
        MemSize count = 0;
        for (MemSize i = 0; i < str.length(); ++i) {
            unsigned char c = str[i];
            if ((c & 0b11000000) != 0b10000000) ++count; // count leading bytes
        }
        return count;
    }

} // namespace Rebel::Core


namespace std {
    template<>
    struct hash<Rebel::Core::String> {
        Rebel::Core::MemSize operator()(const Rebel::Core::String& s) const noexcept {
            return s.hash();
        }
    };
}


namespace fmt {
    template<>
    struct formatter<Rebel::Core::String> {
        // parse is unchanged
        constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(const Rebel::Core::String& s, FormatContext& ctx) const { // <-- note 'const'
            return format_to(ctx.out(), "{}", s.c_str());
        }
    };
}




namespace YAML {
    template<>
    struct convert<Rebel::Core::String> {
        static Node encode(const Rebel::Core::String& rhs) {
            return Node(rhs.c_str()); // encode as plain string
        }

        static bool decode(const Node& node, Rebel::Core::String& rhs) {
            if (!node.IsScalar()) return false;
            rhs = Rebel::Core::String(node.Scalar().c_str());
            return true;
        }
    };
}

