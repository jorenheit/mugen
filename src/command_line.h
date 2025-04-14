class CommandLine {
public: 
  using CommandReturn = std::pair<bool, bool>;
  using CommandArgs = std::vector<std::string>;
  using CommandFunction = std::function<CommandReturn (CommandArgs const &)>;
  
private:
  std::unordered_map<std::string, CommandFunction> cmdMap;
  
  template <typename Return, typename Dummy = void>
  struct AddHelper;
  
  template <typename Dummy>
  struct AddHelper<void, Dummy> {
    template <typename Callable>
    static CommandReturn _return(Callable &&fun, CommandArgs const &args) {
      fun(args);
      return {false, false};
    }
  };
  
  template <typename Dummy>
  struct AddHelper<CommandReturn, Dummy> {
    template <typename Callable>
    static CommandReturn _return(Callable &&fun, CommandArgs const &args) {
      return fun(args);
    } 
  };
  
public:
  template <typename Callable>
  void add(std::string const &cmdName, Callable &&fun) {
    assert(!cmdMap.contains(cmdName) && "Duplicate command");
    
    cmdMap.insert({cmdName, [&](CommandArgs const &args) -> CommandReturn {
      return AddHelper<decltype(fun(args))>::_return(fun, args);
    }});
  }
  
  template <typename Callable>
  void add(std::initializer_list<std::string> const &aliases, Callable &&fun) {
    for (std::string const &cmdName: aliases) {
      add(cmdName, std::forward<Callable>(fun));
    }
  }
  
  CommandReturn exec(std::vector<std::string> const &args) {
    assert(args.size() > 0 && "empty arg list");
    if (!cmdMap.contains(args[0])) {
      debug_error(args[0], "Unknown command.");
      return {false, false};
    }
    return (cmdMap.find(args[0])->second)(args);
  }
};