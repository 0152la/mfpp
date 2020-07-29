#include "srcHelperFunctions.hpp"

namespace fuzz_helpers {

void
CHECK_CONDITION(bool condition, std::string msg)
{
    if (!condition)
    {
        std::cout << "ASSERTION FAILED: " << msg << std::endl;
        exit(1);
    }
}

bool
EMIT_PASS_START_DEBUG(clang::CompilerInstance& ci, std::string pass_name)
{
    std::chrono::duration<double> from_start = std::chrono::system_clock::now() - START_TIME;
    //std::string from_start_str(from_start.count());
    //from_start_str = from_start_str.substr(0, from_start_str.length());
    //from_start_str = "\033[1m\033[31m" + from_start_str + "\033[m";
    std::cout << "[" << "\033[1m\033[31m" << from_start.count() << "\033[m" << "]";
    std::cout << "[" << pass_name << "] Parsing input file ";
    std::cout << ci.getSourceManager().getFileEntryForID(
        ci.getSourceManager().getMainFileID())->getName().str()
        << std::endl;
    return true;
}

} // namespace fuzz_helpers

//void
//EMIT_PASS_DEBUG(const std::string& pass_name, const clang::Rewriter& pass_rw)
//{
    //std::error_code ec;
    //int fd;
    //llvm::sys::fs::createTemporaryFile("", ".cpp", fd, rewritten_input_file);
    //llvm::raw_fd_ostream rif_rfo(fd, true);
    //pass_rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
//}

