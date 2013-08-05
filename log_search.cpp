/* Copyright 2013 Tom Metge

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/ZNCString.h>

#include <set>

class CLogSearch : public CModule {
  public:
    virtual ~CLogSearch() {}

    virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
      return true;
    }

    virtual void OnModCommand(const CString& sCommand) {
      if (sCommand.Equals("HELP")) {
        PutModule("Commands: HELP, SEARCH <query>");
        PutModule("Command: SEARCH <query>");
        PutModule("  Search for <query> in the backlog");
        PutModule("Command: SEARCH <channel> <query>");
        PutModule("  Search for <query> in logs of <channel>");
        PutModule("Command: SEARCH <user> <query>");
        PutModule("  Search for <query> in logs of privmsg to/from <user>");
      } else if (sCommand.Token(0).Equals("SEARCH")) {
        Search(sCommand);
      }
    }

    void Search(const CString& raw_query) {
      CString query = raw_query.LeftChomp_n(raw_query.Token(0).size() + 1);
      if (query.empty()) {
        PutModule("Invalid query");
        return;
      }

      std::set<CString> channels;
      std::set<CString> nicks;
      GetLoggedChannelsAndNicks(GetNetwork()->GetName(), channels, nicks);

      CString chan_nick = query.Token(0);
      CString search_path;
      if (channels.count(chan_nick) > 0 || nicks.count(chan_nick) > 0) {
        // grep in specific channel/nick
        PutModule("Searching in channel/nick (" + chan_nick + ")...");
        search_path = GetSearchPath(GetUser()->GetUserName(),
                                    GetNetwork()->GetName(),
                                    chan_nick);
        query = query.LeftChomp_n(chan_nick.size() + 1);
      } else {
        // general grep (server only)
        PutModule("Searching all channels/nicks");
        search_path = GetSearchPath(GetUser()->GetUserName(),
                                    GetNetwork()->GetName());
      }
      VCString results = Grep(query, search_path);
      for (unsigned int i = 0; i < 10; i++) {
        if (results.size() <= i) break;
        PutModule(results[i]);
      }
      PutModule("Search complete.");
    }

    MODCONSTRUCTOR(CLogSearch)
    {}

  private:
    CString GetLogDataDir() {
      CString data_dir = GetModDataDir();
      // Posix only!
      VCString dir_tokens;
      data_dir.Split("/", dir_tokens);
      dir_tokens.pop_back();
      dir_tokens.pop_back();
      dir_tokens.push_back("moddata");
      dir_tokens.push_back("log");

      CString log_data_dir;
      for (unsigned int i = 0; i < dir_tokens.size(); i++) {
        log_data_dir.append(dir_tokens[i]);
        if (i != dir_tokens.size() - 1) {
          log_data_dir.append("/");
        }
      }

      return log_data_dir;
    }

    VCString GetLogFiles() {
      VCString files;

      CString log_dir = GetLogDataDir();
      DIR *dir = opendir(log_dir.c_str());
      if (dir == NULL) {
        PutModule("Failed to open log directory");
        return files;
      }

      struct dirent *entry;
      do {
        // NOLINT(runtime/threadsafe_fn)
        if ((entry = readdir(dir)) != NULL) {
          files.push_back(CString(entry->d_name));
        }
      } while (entry != NULL);

      if (errno != 0) {
        PutModule("Error listing log directory: " + CString(errno));
      }

      (void)closedir(dir);

      return files;
    }

    void GetLoggedChannelsAndNicks(const CString& network,
                                   std::set<CString>& channels,
                                   std::set<CString>& nicks)
    {
      VCString files = GetLogFiles();
      for (unsigned int i = 0; i < files.size(); i++) {
        CString file = files[i];
        VCString tokens;
        file.Split("_", tokens);

        if (tokens.size() < 4) continue;
        if (!tokens[1].Equals(network)) continue;

        CString chan_nick = tokens[2];
        if (chan_nick.Left(1).Equals("#")) {
          channels.insert(chan_nick);
        } else {
          nicks.insert(chan_nick);
        }
      }
    }

    CString GetSearchPath(const CString& username,
                          const CString& network,
                          const CString& nick_chan)
    {
      return CString(GetLogDataDir() + "/" +
                     username + "_" +
                     network + "_" +
                     nick_chan + "*");
    }

    CString GetSearchPath(const CString& username,
                          const CString& network)
    {
      return GetSearchPath(username, network, "");
    }

    VCString Grep(const CString& query, const CString& path) {
      VCString results;

      CString command = "grep '" + query + "' " + path;

      FILE *fp;
      fp = popen(command.c_str(), "r");
      if (fp == NULL) {
        PutModule("Failed to launch grep command");
        return results;
      }

      char result[1024];
      while (fgets(result, 1024, fp) != NULL) {
        results.push_back(FormatGrepResult(result));
      }

      pclose(fp);

      return results;
    }

    CString FormatGrepResult(const CString& raw_result) {
      // strip all but the date out of the path prefix
      CString raw_path = raw_result.Token(0);
      VCString path_tokens;
      size_t path_len = raw_path.Split("_", path_tokens);
      CString date = path_tokens[path_len - 1].RightChomp_n(15);
      return CString(date + ": " +
                     raw_result.LeftChomp_n(raw_path.size() - 10));
    }
};

template<> void TModInfo<CLogSearch>(CModInfo& Info) {
  Info.AddType(CModInfo::UserModule);
}

MODULEDEFS(CLogSearch, "Search log")
