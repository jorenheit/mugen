class CommandLine {
public: 
  using CommandReturn = std::pair<bool, bool>;
  using CommandArgs = std::vector<std::string>;
  using CommandFunction = std::function<CommandReturn (CommandArgs const &)>;
  
private:
  std::map<std::string, CommandFunction> cmdMap;
  std::map<std::string, std::string> descriptionMap;
  std::map<std::string, std::string> helpMap;
  std::map<std::string, std::vector<std::string>> aliasMap;
  
  template <typename Callable, typename Ret = std::invoke_result_t<Callable, CommandArgs>>
  void add(std::string const &cmdName, Callable&& fun, std::string const &description, 
           std::string const &help, std::string const &aliasTarget) {
           
    assert(!cmdMap.contains(cmdName) && "Duplicate command");
    
    cmdMap.insert({cmdName, [=](CommandArgs const &args) -> CommandReturn {
      if constexpr (std::is_same_v<Ret, void>) {
        fun(args);
        return {false, false};
      }
      else if constexpr (std::is_same_v<Ret, bool>) {
        return {true, fun(args)};
      }
      UNREACHABLE;
    }});
    
    helpMap.insert({cmdName, help});
    if (cmdName == aliasTarget) {
      descriptionMap.insert({cmdName, description});
    }
    else {
      aliasMap[aliasTarget].push_back(cmdName);
    }
  }
  
public:
  template <typename Callable>
  void add(std::initializer_list<std::string> const &aliases, Callable &&fun, 
           std::string const &description, std::string const &help = "") {
    std::vector aliasVec = aliases;
    for (std::string const &cmdName: aliasVec) {
      add(cmdName, std::forward<Callable>(fun), description, help, aliasVec[0]);
    }
  }
  
  CommandReturn exec(CommandArgs const &args) {
    assert(args.size() > 0 && "empty arg list");
    if (!cmdMap.contains(args[0])) {
      debug_error(args[0], "Unknown command.");
      return {false, false};
    }
    return (cmdMap.find(args[0])->second)(args);
  }
  
  void printHelp() {
    std::vector<std::string> commandStrings;
    std::vector<std::string> descriptions;
    size_t maxLength = 0;
    for (auto const &[cmd, description]: descriptionMap) {
      std::string str = cmd;
      for (auto const &alias: aliasMap[cmd])
        str += "|" + alias;
      
      if (str.length() > maxLength) maxLength = str.length();
      commandStrings.push_back(str);
      descriptions.push_back(description);
    }
  
    assert(commandStrings.size() == descriptions.size());
    
    std::cout << "\nAvailable commands:\n";
    for (size_t idx = 0; idx != commandStrings.size(); ++idx) {
      std::cout << std::setw(maxLength + 2) << std::setfill(' ') << commandStrings[idx] 
                << " - " << descriptions[idx] << '\n';
    }
    
    std::cout << "\nType \"help <command>\" for more information about a specific command.\n\n";
  }
  
  void printHelp(std::string const &cmd) {
    if (!helpMap.contains(cmd)) {
      debug_error(cmd, "Unknown command.");
      return;
    }
    
    std::string const &help = helpMap.find(cmd)->second;
    if (help.empty()) {
      std::cout << "No additional help available for command \"" << cmd << "\".\n";
      return;
    }
    
    std::cout << '\n' << help << '\n';
  }
};
