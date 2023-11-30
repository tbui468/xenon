#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>

class Matcher {
public:
    Matcher(const std::string& pattern) {
        Compiler c(pattern);
        //nfa_ = c.CompileForLike();
        nfa_ = c.CompileForSimilar();
    }

    bool Match(const std::string& str) {
        std::vector<NfaState*> current_states;
        std::vector<NfaState*> visited;
        AddNextState(nfa_.start, current_states, visited);

        for (const char& c: str) {
            std::vector<NfaState*> next_states;
            for (NfaState* s: current_states) {
                std::unordered_map<char, std::shared_ptr<NfaState>>::iterator it = s->symbol_transition.find(c);
                std::unordered_map<char, std::shared_ptr<NfaState>>::iterator it_any = s->symbol_transition.find('_');
                std::vector<NfaState*> placeholder;
                if (it != s->symbol_transition.end()) {
                    AddNextState(it->second, next_states, placeholder);
                } else if (it_any != s->symbol_transition.end()) {
                    AddNextState(it_any->second, next_states, placeholder);
                }
            }
            current_states = next_states;
        }

        for (NfaState* s: current_states) {
            if (s->is_end) return true;
        }

        return false;
    }

private:
    struct NfaState {
    public:
        NfaState(bool is_end): is_end(is_end) {}
    public:
        bool is_end;
        std::unordered_map<char, std::shared_ptr<NfaState>> symbol_transition; //could replace this with an std::pair
        std::vector<std::shared_ptr<NfaState>> epsilon_transitions;
    };

    struct Nfa {
        std::shared_ptr<NfaState> start;
        std::shared_ptr<NfaState> end;
    };

    class Compiler {
    public:
        Compiler(const std::string& pattern): pattern_(pattern) {}
        Nfa CompileForLike() {
            Nfa root = CompileLikeBase();
            while (!AtEnd()) {
                root = MakeConcat(root, CompileLikeBase());
            }
            return root;
        }
        Nfa CompileForSimilar() {
            Nfa root = CompileAlternation();
            while (!AtEnd()) {
                root = MakeConcat(root, CompileAlternation());
            }
            return root;
        }
    private:
        //functions for 'like' matching
        Nfa CompileLikeBase() {
            char c = NextChar();
            switch (c) {
                case '%':   return MakeClosure(MakeSymbol('_'));
                case '_':   return MakeSymbol('_');
                default:    return MakeSymbol(c);
            }
        }

        //functions for 'similar to' matching
        Nfa CompileAlternation() {
            Nfa left = CompileConcat();
            while (PeekChar('|')) {
                NextChar();
                left = MakeUnion(left, CompileConcat());
            }
            return left;
        }
        Nfa CompileConcat() {
            Nfa left = CompileDuplication();
            while (!AtEnd() && !PeekMetaChar()) {
                left = MakeConcat(left, CompileDuplication());
            }
            return left;
        }
        Nfa CompileDuplication() {
            Nfa left = CompileAtomic();
            while (PeekDupChar()) {
                char next = NextChar();
                switch (next) {
                    case '*':
                        left = MakeClosure(left);
                        break;
                    case '+':
                        left = MakeOneOrMore(left);
                        break;
                    case '?':
                        left = MakeZeroOrOne(left);
                        break;
                    case '{': {
                        int m = ParseInt();
                        if (PeekChar('}')) {
                            NextChar(); //} 
                            left = MakeExactlyM(left, m);
                        } else if (PeekChar(',')) {
                            NextChar(); //,
                            if (PeekChar('}')) {
                                NextChar(); //}
                                left = MakeMOrMore(left, m);
                            } else { //next is integer
                                int n = ParseInt();
                                NextChar(); //}
                                left = MakeMToN(left, m, n);
                            }
                        }
                        break;
                    }
                    default:
                        std::cout << "Operator not implemented\n";
                        break;
                }
            }
            return left;
        }
        Nfa CompileAtomic() {
            char next = NextChar();
            switch (next) {
                case '(': {
                    size_t start = idx_;
                    while (!PeekChar(')')) {
                        NextChar();
                    }
                    size_t end = idx_;
                    NextChar();
                    Compiler c(pattern_.substr(start, end - start));
                    return c.CompileForSimilar();
                }
                case '%':   return MakeClosure(MakeSymbol('_'));
                case '_':   return MakeSymbol('_');
                default:    return MakeSymbol(next);
            }  
        }
        bool PeekMetaChar() {
            return PeekChar('|') ||
                   PeekChar('*') ||
                   PeekChar('_') ||
                   PeekChar('%') ||
                   PeekChar('+') ||
                   PeekChar('?') ||
                   PeekChar('(') ||
                   PeekChar(')');
        }

        bool PeekDupChar() {
            return PeekChar('*') ||
                   PeekChar('+') ||
                   PeekChar('?') ||
                   PeekChar('{');
        }
        bool PeekChar(char c) {
            return !AtEnd() && pattern_.at(idx_) == c;
        }
        char PeekChar() {
            return pattern_.at(idx_);
        }
        char NextChar() {
            return pattern_.at(idx_++);
        }
        int ParseInt() {
            size_t start = idx_;
            while (!AtEnd() && IsNumeric(PeekChar())) {
                NextChar();
            }
            return std::stoi(pattern_.substr(start, idx_ - start));
        }
        bool IsNumeric(char c) {
            return '0' <= c && c <= '9';
        }
        bool AtEnd() {
            return idx_ >= pattern_.size();
        }

        //functions for NFAs
        void AddEpsilonTransition(std::shared_ptr<NfaState> from, std::shared_ptr<NfaState> to) {
            from->epsilon_transitions.push_back(to);
        }

        void AddSymbolTransition(std::shared_ptr<NfaState> from, std::shared_ptr<NfaState> to, char symbol) {
            from->symbol_transition.insert({ symbol, to });
        }

        //TODO: look at this - not really used in the way we are compiling right now
        Nfa MakeEpsilon() {
            std::shared_ptr<NfaState> start = std::make_shared<NfaState>(false);
            std::shared_ptr<NfaState> end = std::make_shared<NfaState>(true);
            AddEpsilonTransition(start, end);
            return { start, end };
        }

        Nfa MakeSymbol(char symbol) {
            std::shared_ptr<NfaState> start = std::make_shared<NfaState>(false);
            std::shared_ptr<NfaState> end = std::make_shared<NfaState>(true);
            AddSymbolTransition(start, end, symbol);
            return { start, end };
        }
        Nfa MakeConcat(Nfa first, Nfa second) {
            AddEpsilonTransition(first.end, second.start);
            first.end->is_end = false;
            return { first.start, second.end };
        }
        Nfa MakeClosure(Nfa nfa) {
            std::shared_ptr<NfaState> start = std::make_shared<NfaState>(false);
            std::shared_ptr<NfaState> end = std::make_shared<NfaState>(true);

            AddEpsilonTransition(start, end);
            AddEpsilonTransition(start, nfa.start);

            AddEpsilonTransition(nfa.end, end);
            AddEpsilonTransition(nfa.end, nfa.start);
            nfa.end->is_end = false;
            return { start, end };
        }
        Nfa MakeUnion(Nfa first, Nfa second) {
            std::shared_ptr<NfaState> start = std::make_shared<NfaState>(false);
            AddEpsilonTransition(start, first.start);
            AddEpsilonTransition(start, second.start);

            std::shared_ptr<NfaState> end = std::make_shared<NfaState>(true);
            AddEpsilonTransition(first.end, end);
            first.end->is_end = false;
            AddEpsilonTransition(second.end, end);
            second.end->is_end = false;

            return { start, end };
        }
        Nfa MakeOneOrMore(Nfa nfa) {
            return MakeConcat(nfa, MakeClosure(nfa));
        }
        Nfa MakeZeroOrOne(Nfa nfa) {
            std::shared_ptr<NfaState> start = std::make_shared<NfaState>(false);
            std::shared_ptr<NfaState> end = std::make_shared<NfaState>(true);

            AddEpsilonTransition(start, end);
            AddEpsilonTransition(start, nfa.start);

            AddEpsilonTransition(nfa.end, end);
            nfa.end->is_end = false;
            return { start, end };
        }
        Nfa MakeExactlyM(Nfa nfa, int m) {
            Nfa result = nfa;

            for (int i = 0; i < m - 1; i++) {
                result = MakeConcat(result, nfa);
            }

            return result;
        }
        Nfa MakeMOrMore(Nfa nfa, int m) {
            return MakeConcat(MakeExactlyM(nfa, m), MakeClosure(nfa));
        }
        Nfa MakeMToN(Nfa nfa, int m, int n) {
            Nfa result = MakeExactlyM(nfa, m);

            for (int i = 0; i < n - m; i++) {
                result = MakeConcat(result, MakeZeroOrOne(nfa)); 
            }

            return result;
        }

    private:
        std::string pattern_;
        size_t idx_ {0};
    };    

    void AddNextState(std::shared_ptr<NfaState> state, std::vector<NfaState*>& next_states, std::vector<NfaState*>& visited_states) {
        if (!state->epsilon_transitions.empty()) {
            for (std::shared_ptr<NfaState> s: state->epsilon_transitions) {
                if (std::find(visited_states.begin(), visited_states.end(), s.get()) == visited_states.end()) {
                    visited_states.push_back(s.get());
                    AddNextState(s, next_states, visited_states);
                }
            }
        } else {
            next_states.push_back(state.get());
        }
    }
private:
    Nfa nfa_;
};

int main() {
    /*
    Matcher m("_ab%");
    std::cout << m.Match("bba") << std::endl;
    std::cout << m.Match("ab") << std::endl;
    std::cout << m.Match("zab") << std::endl;
    std::cout << m.Match("caaaaaaab") << std::endl;
    std::cout << m.Match("cabaaaa") << std::endl;*/
    Matcher m("a{2}");
    std::cout << m.Match("a") << std::endl;
    std::cout << m.Match("aa") << std::endl;
    std::cout << m.Match("aaa") << std::endl;
}
