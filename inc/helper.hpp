#ifndef SSPROJ_HELPER_HPP
#define SSPROJ_HELPER_HPP

void err(std::string msg);
void verrInvalidInstruction(struct instruction inst);
void verrInvalidDirective(struct directive dir);
void verrInvalidLabel(char* name,assemblerContext* cx);
void verrNoSection(assemblerContext* cx,std::string msg);
void verrSkipNegative(struct directive dir);
void verrUndefinedSym(assemblerContext* cx);
std::pair<int,std::string> verrReloc(std::string name,assemblerContext* cx);
void vwarnOverflow(int ilit);
void vwarEqu(assemblerContext* cx);
void vwarnRelAbs(std::string name);
void vwarnGlobExt(std::string name);
void vwarnGlobalSection(std::string name,int info);

void emitWord(assemblerContext* cx,int word,int endianess);

#endif //SSPROJ_HELPER_HPP
