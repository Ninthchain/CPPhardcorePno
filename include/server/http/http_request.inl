

#if defined(FMT_VERSION)
template <> struct fmt::formatter<CppServer::HTTP::HTTPRequest> : ostream_formatter {};
#endif

template <>
struct std::hash<CppServer::HTTP::HTTPRequest>
{
    typedef CppServer::HTTP::HTTPRequest argument_type;
    typedef size_t result_type;

    result_type operator() (const argument_type& value) const
    {
        result_type result = 17;
        result = result * 31 + std::hash<std::string>()(value.cache());
        return result;
    }
};
