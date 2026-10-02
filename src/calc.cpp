#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <limits>
#include <functional>

const double PI = 3.1415926535897932384626433832795;
const double E = 2.7182818284590452353602874713527;

typedef std::complex<double> Complex;

std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a]))
        ++a;

    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        --b;

    return s.substr(a, b - a);

}

std::string lower(const std::string& s) {
    std::string r = s;

    for (size_t i = 0; i < r.size(); ++i)
        r[i] = (char)std::tolower((unsigned char)r[i]);

    return r;

}

double number(const std::string& s) {
    std::string t = trim(s);
    size_t p = 0;
    double v = std::stod(t, &p);

    if (p != t.size())
        throw std::runtime_error("invalid number");

    return v;

}

std::string fmt(double x, int precision = 8) {
    if (std::abs(x) < 1e-12)
        x = 0;

    std::ostringstream o;
    o << std::fixed << std::setprecision(precision) << x;

    std::string r = o.str();

    while (!r.empty() && r[r.size() - 1] == '0')
        r.erase(r.size() - 1);

    if (!r.empty() && r[r.size() - 1] == '.')
        r.erase(r.size() - 1);

    if (r == "-0")
        r = "0";

    return r.empty() ? "0" : r;

}

std::string complexText(const Complex& z) {
    double a = z.real();
    double b = z.imag();

    if (std::abs(b) < 1e-10)
        return fmt(a);

    if (std::abs(a) < 1e-10)
        return fmt(b) + "i";

    if (b >= 0)
        return fmt(a) + " + " + fmt(b) + "i";

    return fmt(a) + " - " + fmt(-b) + "i";

}

class Parser {
    std::string s;
    size_t pos;
    double x;

public:
    Parser() : pos(0), x(0) {}

    Complex evaluate(const std::string& expression, double xv = 0) {
        s = expression;
        pos = 0;
        x = xv;

        Complex r = expressionParser();

        skip();

        if (pos != s.size())
            throw std::runtime_error("unexpected input");

        return r;
    }

    bool hasX(const std::string& expression) {
        for (size_t i = 0; i < expression.size(); ++i) {
            if (std::tolower((unsigned char)expression[i]) == 'x')
                return true;
        }

        return false;
    }

private:
    void skip() {
        while (pos < s.size() && std::isspace((unsigned char)s[pos]))
            ++pos;
    }

    bool match(char c) {
        skip();

        if (pos < s.size() && s[pos] == c) {
            ++pos;
            return true;
        }

        return false;
    }

    std::string identifier() {
        skip();

        std::string r;

        while (
            pos < s.size() &&
            std::isalpha((unsigned char)s[pos])
        )
            r += s[pos++];

            return lower(r);
    }

    bool startsImplicitMultiplication() {
        skip();

        if (pos >= s.size())
            return false;

        char c = s[pos];

        if (c == '(')
            return true;

        if (std::isalpha((unsigned char)c))
            return true;

        if (std::isdigit((unsigned char)c) || c == '.') {
            if (
                pos > 0 &&
                (
                    std::isdigit((unsigned char)s[pos - 1]) ||
                    s[pos - 1] == '.'
                )
            )
                return false;

                return true;
        }

        return false;
    }

    Complex expressionParser() {
        Complex r = term();

        while (true) {
            if (match('+'))
                r += term();
            else if (match('-'))
                r -= term();
            else
                break;
        }

        return r;
    }

    Complex term() {
        Complex r = unary();

        while (true) {
            if (match('*')) {
                if (match('*'))
                    r = std::pow(r, unary());
                else
                    r *= unary();
            }
            else if (match('/')) {
                Complex d = unary();

                if (std::abs(d) < 1e-15)
                    throw std::runtime_error("division by zero");

                r /= d;
            }
            else if (startsImplicitMultiplication()) {
                r *= unary();
            }
            else {
                break;
            }
        }

        return r;
    }

    Complex unary() {
        skip();

        if (match('+'))
            return unary();

        if (match('-'))
            return -unary();

        return power();
    }

    Complex power() {
        Complex r = postfix();

        skip();

        if (match('^'))
            r = std::pow(r, unary());

        return r;
    }

    Complex postfix() {
        Complex r = primary();

        while (true) {
            skip();

            if (!match('!'))
                break;

            r = gamma(r + Complex(1.0, 0));
        }

        return r;
    }

    Complex primary() {
        skip();

        if (match('(')) {
            Complex r = expressionParser();

            if (!match(')'))
                throw std::runtime_error(
                    "missing closing parenthesis"
                );

            return r;
        }

        if (pos >= s.size())
            throw std::runtime_error(
                "unexpected end of expression"
            );

        if (
            std::isdigit((unsigned char)s[pos]) ||
            s[pos] == '.'
        ) {
            size_t start = pos;
            bool digits = false;
            bool dot = false;
            bool exponent = false;

            while (pos < s.size()) {
                char c = s[pos];

                if (std::isdigit((unsigned char)c)) {
                    digits = true;
                    ++pos;
                    continue;
                }

                if (c == '.' && !dot && !exponent) {
                    dot = true;
                    ++pos;
                    continue;
                }

                if (
                    (c == 'e' || c == 'E') &&
                    !exponent &&
                    digits
                ) {
                    exponent = true;
                    ++pos;

                    if (
                        pos < s.size() &&
                        (
                            s[pos] == '+' ||
                            s[pos] == '-'
                        )
                    )
                        ++pos;

                        continue;
                }

                break;
            }

            try {
                return Complex(
                    std::stod(
                        s.substr(
                            start,
                            pos - start
                        )
                    ),
                    0
                );
            }
            catch (...) {
                throw std::runtime_error(
                    "invalid number"
                );
            }
        }

        if (std::isalpha((unsigned char)s[pos])) {
            std::string name = identifier();

            if (name == "x")
                return Complex(x, 0);

            if (name == "i")
                return Complex(0, 1);

            if (name == "pi")
                return Complex(PI, 0);

            if (name == "e")
                return Complex(E, 0);

            if (!match('('))
                throw std::runtime_error(
                    "unknown variable: " + name
                );

            std::vector<Complex> args;

            skip();

            if (!match(')')) {
                while (true) {
                    args.push_back(
                        expressionParser()
                    );

                    skip();

                    if (match(')'))
                        break;

                    if (!match(','))
                        throw std::runtime_error(
                            "expected ',' or ')'"
                        );
                }
            }

            return callFunction(name, args);
        }

        throw std::runtime_error(
            "unexpected input"
        );
    }

    double realArg(
        const Complex& z,
        const std::string& name
    ) {
        if (std::abs(z.imag()) > 1e-12)
            throw std::runtime_error(
                name + " requires a real argument"
            );

        return z.real();
    }

    void requireArgs(
        const std::string& name,
        const std::vector<Complex>& args,
        size_t count
    ) {
        if (args.size() != count)
            throw std::runtime_error(
                name +
                " expects " +
                std::to_string(count) +
                " argument" +
                (count == 1 ? "" : "s")
            );
    }

    Complex gamma(const Complex& z) {
        static const double c[] = {
            676.5203681218851,
            -1259.1392167224028,
            771.32342877765313,
            -176.61502916214059,
            12.507343278686905,
            -0.13857109526572012,
            9.9843695780195716e-6,
            1.5056327351493116e-7
        };

        if (
            std::abs(z.imag()) < 1e-14 &&
            z.real() > 0 &&
            z.real() <= 171 &&
            z.real() == std::floor(z.real())
        ) {
            return Complex(
                std::tgamma(z.real()),
                           0
            );
        }

        if (z.real() < 0.5) {
            return
            Complex(PI, 0) /
            (
                std::sin(
                    Complex(PI, 0) * z
                ) *
                gamma(
                    Complex(1, 0) - z
                )
            );
        }

        Complex y = z - Complex(1, 0);
        Complex a(0.99999999999980993, 0);

        for (int i = 0; i < 8; ++i) {
            a +=
            Complex(c[i], 0) /
            (
                y +
                Complex(
                    i + 1,
                    0
                )
            );
        }

        Complex t = y + Complex(7.5, 0);

        return
        std::sqrt(
            Complex(
                2.0 * PI,
                0
            )
        ) *
        std::pow(
            t,
            y + Complex(0.5, 0)
        ) *
        std::exp(-t) *
        a;
    }

    Complex callFunction(
        const std::string& name,
        const std::vector<Complex>& args
    ) {
        if (name == "sin") {
            requireArgs(name, args, 1);
            return std::sin(args[0]);
        }

        if (name == "cos") {
            requireArgs(name, args, 1);
            return std::cos(args[0]);
        }

        if (name == "tan") {
            requireArgs(name, args, 1);
            return std::tan(args[0]);
        }

        if (name == "asin") {
            requireArgs(name, args, 1);
            return std::asin(args[0]);
        }

        if (name == "acos") {
            requireArgs(name, args, 1);
            return std::acos(args[0]);
        }

        if (name == "atan") {
            requireArgs(name, args, 1);
            return std::atan(args[0]);
        }

        if (name == "atan2") {
            requireArgs(name, args, 2);

            return Complex(
                std::atan2(
                    realArg(args[0], name),
                           realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "sinh") {
            requireArgs(name, args, 1);
            return std::sinh(args[0]);
        }

        if (name == "cosh") {
            requireArgs(name, args, 1);
            return std::cosh(args[0]);
        }

        if (name == "tanh") {
            requireArgs(name, args, 1);
            return std::tanh(args[0]);
        }

        if (name == "asinh") {
            requireArgs(name, args, 1);
            return std::asinh(args[0]);
        }

        if (name == "acosh") {
            requireArgs(name, args, 1);
            return std::acosh(args[0]);
        }

        if (name == "atanh") {
            requireArgs(name, args, 1);
            return std::atanh(args[0]);
        }

        if (name == "sqrt") {
            requireArgs(name, args, 1);
            return std::sqrt(args[0]);
        }

        if (name == "cbrt") {
            requireArgs(name, args, 1);

            if (std::abs(args[0].imag()) < 1e-12)
                return Complex(
                    std::cbrt(args[0].real()),
                               0
                );

            return std::pow(
                args[0],
                Complex(1.0 / 3.0, 0)
            );
        }

        if (name == "pow") {
            requireArgs(name, args, 2);
            return std::pow(args[0], args[1]);
        }

        if (name == "exp") {
            requireArgs(name, args, 1);
            return std::exp(args[0]);
        }

        if (name == "exp2") {
            requireArgs(name, args, 1);
            return std::pow(
                Complex(2, 0),
                            args[0]
            );
        }

        if (name == "expm1") {
            requireArgs(name, args, 1);

            if (std::abs(args[0].imag()) < 1e-12)
                return Complex(
                    std::expm1(args[0].real()),
                               0
                );

            return std::exp(args[0]) - Complex(1, 0);
        }

        if (
            name == "ln" ||
            name == "log"
        ) {
            requireArgs(name, args, 1);

            if (name == "ln")
                return std::log(args[0]);

            return
            std::log(args[0]) /
            std::log(Complex(10, 0));
        }

        if (name == "log10") {
            requireArgs(name, args, 1);

            return
            std::log(args[0]) /
            std::log(Complex(10, 0));
        }

        if (name == "log2") {
            requireArgs(name, args, 1);

            return
            std::log(args[0]) /
            std::log(Complex(2, 0));
        }

        if (name == "log1p") {
            requireArgs(name, args, 1);

            if (std::abs(args[0].imag()) < 1e-12)
                return Complex(
                    std::log1p(args[0].real()),
                               0
                );

            return std::log(
                Complex(1, 0) + args[0]
            );
        }

        if (name == "abs") {
            requireArgs(name, args, 1);

            return Complex(
                std::abs(args[0]),
                           0
            );
        }

        if (name == "fabs") {
            requireArgs(name, args, 1);

            return Complex(
                std::fabs(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "hypot") {
            requireArgs(name, args, 2);

            return Complex(
                std::hypot(
                    realArg(args[0], name),
                           realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "real") {
            requireArgs(name, args, 1);
            return Complex(args[0].real(), 0);
        }

        if (name == "imag") {
            requireArgs(name, args, 1);
            return Complex(args[0].imag(), 0);
        }

        if (name == "conj") {
            requireArgs(name, args, 1);
            return std::conj(args[0]);
        }

        if (name == "arg") {
            requireArgs(name, args, 1);

            return Complex(
                std::arg(args[0]),
                           0
            );
        }

        if (name == "norm") {
            requireArgs(name, args, 1);

            return Complex(
                std::norm(args[0]),
                           0
            );
        }

        if (
            name == "gamma" ||
            name == "tgamma"
        ) {
            requireArgs(name, args, 1);
            return gamma(args[0]);
        }

        if (name == "lgamma") {
            requireArgs(name, args, 1);

            return Complex(
                std::lgamma(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "erf") {
            requireArgs(name, args, 1);

            return Complex(
                std::erf(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "erfc") {
            requireArgs(name, args, 1);

            return Complex(
                std::erfc(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "ceil") {
            requireArgs(name, args, 1);

            return Complex(
                std::ceil(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "floor") {
            requireArgs(name, args, 1);

            return Complex(
                std::floor(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "trunc") {
            requireArgs(name, args, 1);

            return Complex(
                std::trunc(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "round") {
            requireArgs(name, args, 1);

            return Complex(
                std::round(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "nearbyint") {
            requireArgs(name, args, 1);

            return Complex(
                std::nearbyint(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "rint") {
            requireArgs(name, args, 1);

            return Complex(
                std::rint(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "fmod") {
            requireArgs(name, args, 2);

            return Complex(
                std::fmod(
                    realArg(args[0], name),
                          realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "remainder") {
            requireArgs(name, args, 2);

            return Complex(
                std::remainder(
                    realArg(args[0], name),
                               realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "fmax") {
            requireArgs(name, args, 2);

            return Complex(
                std::fmax(
                    realArg(args[0], name),
                          realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "fmin") {
            requireArgs(name, args, 2);

            return Complex(
                std::fmin(
                    realArg(args[0], name),
                          realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "fdim") {
            requireArgs(name, args, 2);

            return Complex(
                std::fdim(
                    realArg(args[0], name),
                          realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "copysign") {
            requireArgs(name, args, 2);

            return Complex(
                std::copysign(
                    realArg(args[0], name),
                              realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "fma") {
            requireArgs(name, args, 3);

            return Complex(
                std::fma(
                    realArg(args[0], name),
                         realArg(args[1], name),
                         realArg(args[2], name)
                ),
                0
            );
        }

        if (name == "ldexp") {
            requireArgs(name, args, 2);

            return Complex(
                std::ldexp(
                    realArg(args[0], name),
                           (int)realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "scalbn") {
            requireArgs(name, args, 2);

            return Complex(
                std::scalbn(
                    realArg(args[0], name),
                            (int)realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "ilogb") {
            requireArgs(name, args, 1);

            return Complex(
                (double)std::ilogb(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "logb") {
            requireArgs(name, args, 1);

            return Complex(
                std::logb(
                    realArg(args[0], name)
                ),
                0
            );
        }

        if (name == "nextafter") {
            requireArgs(name, args, 2);

            return Complex(
                std::nextafter(
                    realArg(args[0], name),
                               realArg(args[1], name)
                ),
                0
            );
        }

        if (name == "isfinite") {
            requireArgs(name, args, 1);

            return Complex(
                std::isfinite(
                    realArg(args[0], name)
                ) ? 1.0 : 0.0,
                0
            );
        }

        if (name == "isinf") {
            requireArgs(name, args, 1);

            return Complex(
                std::isinf(
                    realArg(args[0], name)
                ) ? 1.0 : 0.0,
                0
            );
        }

        if (name == "isnan") {
            requireArgs(name, args, 1);

            return Complex(
                std::isnan(
                    realArg(args[0], name)
                ) ? 1.0 : 0.0,
                0
            );
        }

        if (name == "isnormal") {
            requireArgs(name, args, 1);

            return Complex(
                std::isnormal(
                    realArg(args[0], name)
                ) ? 1.0 : 0.0,
                0
            );
        }

        if (name == "signbit") {
            requireArgs(name, args, 1);

            return Complex(
                std::signbit(
                    realArg(args[0], name)
                ) ? 1.0 : 0.0,
                0
            );
        }

        if (name == "fpclassify") {
            requireArgs(name, args, 1);

            return Complex(
                (double)std::fpclassify(
                    realArg(args[0], name)
                ),
                0
            );
        }

        throw std::runtime_error(
            "unknown function: " + name
        );
    }

};

double realEval(
    Parser& p,
    const std::string& expression,
    double x
) {
    Complex z = p.evaluate(expression, x);

    if (std::abs(z.imag()) > 1e-8)
        throw std::runtime_error(
            "expression is not real"
        );

    double result = z.real();

    if (!std::isfinite(result))
        throw std::runtime_error(
            "expression returned a non-finite value"
        );

    return result;

}

double derivative(
    Parser& p,
    const std::string& expression,
    double x
) {
    double h = 1e-5;

    return (
        realEval(p, expression, x + h) -
        realEval(p, expression, x - h)
    ) / (2.0 * h);

}

double secondDerivative(
    Parser& p,
    const std::string& expression,
    double x
) {
    double h = 1e-4;

    return (
        realEval(p, expression, x + h) -
        2.0 * realEval(p, expression, x) +
        realEval(p, expression, x - h)
    ) / (h * h);

}

double finiteSimpson(
    Parser& p,
    const std::string& expression,
    double a,
    double b
) {
    double c = a + (b - a) / 2.0;

    double fa = realEval(p, expression, a);
    double fc = realEval(p, expression, c);
    double fb = realEval(p, expression, b);

    return
    (b - a) / 6.0 *
    (fa + 4.0 * fc + fb);

}

double finiteAdaptive(
    Parser& p,
    const std::string& expression,
    double a,
    double b,
    double eps,
    double whole,
    int depth
) {
    double c = a + (b - a) / 2.0;

    double left =
    finiteSimpson(
        p,
        expression,
        a,
        c
    );

    double right =
    finiteSimpson(
        p,
        expression,
        c,
        b
    );

    double delta = left + right - whole;

    if (
        !std::isfinite(left) ||
        !std::isfinite(right) ||
        !std::isfinite(delta)
    )
        throw std::runtime_error(
            "integral became numerically unstable"
        );

        if (
            depth <= 0 ||
            std::abs(delta) < 15.0 * eps
        )
            return left + right + delta / 15.0;

            return
            finiteAdaptive(
                p,
                expression,
                a,
                c,
                eps / 2.0,
                left,
                depth - 1
            ) +
            finiteAdaptive(
                p,
                expression,
                c,
                b,
                eps / 2.0,
                right,
                depth - 1
            );

}

double finiteIntegrate(
    Parser& p,
    const std::string& expression,
    double a,
    double b,
    double eps = 1e-9
) {
    if (a == b)
        return 0.0;

    if (a > b)
        return -finiteIntegrate(
            p,
            expression,
            b,
            a,
            eps
        );

    double whole =
    finiteSimpson(
        p,
        expression,
        a,
        b
    );

    double result =
    finiteAdaptive(
        p,
        expression,
        a,
        b,
        eps,
        whole,
        20
    );

    if (!std::isfinite(result))
        throw std::runtime_error(
            "integral is not finite"
        );

    return result;

}

enum BoundType {
    FINITE_BOUND,
    POSITIVE_INFINITY,
    NEGATIVE_INFINITY
};

struct Bound {
    BoundType type;
    double value;
};

Bound parseBound(const std::string& input) {
    std::string s = lower(trim(input));

    s.erase(
        std::remove_if(
            s.begin(),
                       s.end(),
                       [](unsigned char c) {
                           return std::isspace(c);
                       }
        ),
        s.end()
    );

    if (
        s == "inf" ||
        s == "+inf" ||
        s == "infinity" ||
        s == "+infinity" ||
        s == "∞"
    )
        return { POSITIVE_INFINITY, 0.0 };

        if (
            s == "-inf" ||
            s == "-infinity" ||
            s == "-∞"
        )
            return { NEGATIVE_INFINITY, 0.0 };

            double v = number(s);

            if (!std::isfinite(v))
                throw std::runtime_error(
                    "invalid integration bound"
                );

            return { FINITE_BOUND, v };

}

double integrateToPositiveInfinity(
    Parser& p,
    const std::string& expression,
    double a,
    double eps = 1e-8
) {
    const double T = 1.0 - 1e-10;

    auto transformed =
    [&](double t) -> double {
        if (t < 0 || t >= 1)
            throw std::runtime_error(
                "invalid infinite parameter"
            );

        double d = 1.0 - t;

        if (d < 1e-12)
            throw std::runtime_error(
                "infinite transformation became unstable"
            );

        double xx = a + t / d;
        double dxdt = 1.0 / (d * d);

        double y =
        realEval(
            p,
            expression,
            xx
        );

        double result = y * dxdt;

        if (!std::isfinite(result))
            throw std::runtime_error(
                "integral became numerically unstable"
            );

        return result;
    };

    std::function<double(double, double)> simpson;
    std::function<double(double, double, double, double, int)> adaptive;

    simpson =
    [&](double left, double right) -> double {
        double c = left + (right - left) / 2.0;

        return
        (right - left) / 6.0 *
        (
            transformed(left) +
            4.0 * transformed(c) +
            transformed(right)
        );
    };

    adaptive =
    [&](double left,
        double right,
        double tolerance,
        double whole,
        int depth) -> double {
            double c = left + (right - left) / 2.0;

            double l = simpson(left, c);
            double r = simpson(c, right);

            double delta = l + r - whole;

            if (!std::isfinite(delta))
                throw std::runtime_error(
                    "integral appears to diverge"
                );

            if (
                depth <= 0 ||
                std::abs(delta) < 15.0 * tolerance
            )
                return l + r + delta / 15.0;

                return
                adaptive(
                    left,
                    c,
                    tolerance / 2.0,
                    l,
                    depth - 1
                ) +
                adaptive(
                    c,
                    right,
                    tolerance / 2.0,
                    r,
                    depth - 1
                );
        };

        double whole = simpson(0.0, T);

        double result =
        adaptive(
            0.0,
            T,
            eps,
            whole,
            20
        );

        if (!std::isfinite(result))
            throw std::runtime_error(
                "integral does not converge"
            );

        return result;

}

double integrateToNegativeInfinity(
    Parser& p,
    const std::string& expression,
    double b,
    double eps = 1e-8
) {
    const double T = 1.0 - 1e-10;

    auto transformed =
    [&](double t) -> double {
        if (t < 0 || t >= 1)
            throw std::runtime_error(
                "invalid infinite parameter"
            );

        double d = 1.0 - t;

        if (d < 1e-12)
            throw std::runtime_error(
                "infinite transformation became unstable"
            );

        double xx = b - t / d;
        double dxdt = 1.0 / (d * d);

        double y =
        realEval(
            p,
            expression,
            xx
        );

        double result = y * dxdt;

        if (!std::isfinite(result))
            throw std::runtime_error(
                "integral became numerically unstable"
            );

        return result;
    };

    std::function<double(double, double)> simpson;
    std::function<double(double, double, double, double, int)> adaptive;

    simpson =
    [&](double left, double right) -> double {
        double c = left + (right - left) / 2.0;

        return
        (right - left) / 6.0 *
        (
            transformed(left) +
            4.0 * transformed(c) +
            transformed(right)
        );
    };

    adaptive =
    [&](double left,
        double right,
        double tolerance,
        double whole,
        int depth) -> double {
            double c = left + (right - left) / 2.0;

            double l = simpson(left, c);
            double r = simpson(c, right);

            double delta = l + r - whole;

            if (!std::isfinite(delta))
                throw std::runtime_error(
                    "integral appears to diverge"
                );

            if (
                depth <= 0 ||
                std::abs(delta) < 15.0 * tolerance
            )
                return l + r + delta / 15.0;

                return
                adaptive(
                    left,
                    c,
                    tolerance / 2.0,
                    l,
                    depth - 1
                ) +
                adaptive(
                    c,
                    right,
                    tolerance / 2.0,
                    r,
                    depth - 1
                );
        };

        double whole = simpson(0.0, T);

        double result =
        adaptive(
            0.0,
            T,
            eps,
            whole,
            20
        );

        if (!std::isfinite(result))
            throw std::runtime_error(
                "integral does not converge"
            );

        return result;

}

double checkedPositiveInfinity(
    Parser& p,
    const std::string& expression,
    double a
) {
    double result =
    integrateToPositiveInfinity(
        p,
        expression,
        a
    );

    const double cutoffs[] = {
        10.0,
        100.0,
        1000.0,
        10000.0
    };

    double previous = 0;
    bool havePrevious = false;
    int stableCount = 0;

    for (int i = 0; i < 4; ++i) {
        double cutoff = cutoffs[i];

        if (cutoff <= a)
            continue;

        double current =
        finiteIntegrate(
            p,
            expression,
            a,
            cutoff,
            1e-7
        );

        if (!std::isfinite(current))
            throw std::runtime_error(
                "integral does not converge"
            );

        if (havePrevious) {
            double difference =
            std::abs(
                current - previous
            );

            double scale =
            std::max(
                1.0,
                std::abs(current)
            );

            if (difference < 1e-5 * scale)
                ++stableCount;
            else
                stableCount = 0;
        }

        previous = current;
        havePrevious = true;
    }

    if (
        stableCount == 0 &&
        std::abs(result) > 1e10
    )
        throw std::runtime_error(
            "integral appears to diverge"
        );

        return result;

}

double checkedNegativeInfinity(
    Parser& p,
    const std::string& expression,
    double b
) {
    double result =
    integrateToNegativeInfinity(
        p,
        expression,
        b
    );

    const double cutoffs[] = {
        10.0,
        100.0,
        1000.0,
        10000.0
    };

    double previous = 0;
    bool havePrevious = false;
    int stableCount = 0;

    for (int i = 0; i < 4; ++i) {
        double left = b - cutoffs[i];

        double current =
        finiteIntegrate(
            p,
            expression,
            left,
            b,
            1e-7
        );

        if (!std::isfinite(current))
            throw std::runtime_error(
                "integral does not converge"
            );

        if (havePrevious) {
            double difference =
            std::abs(
                current - previous
            );

            double scale =
            std::max(
                1.0,
                std::abs(current)
            );

            if (difference < 1e-5 * scale)
                ++stableCount;
            else
                stableCount = 0;
        }

        previous = current;
        havePrevious = true;
    }

    if (
        stableCount == 0 &&
        std::abs(result) > 1e10
    )
        throw std::runtime_error(
            "integral appears to diverge"
        );

        return result;

}

double integrateImproper(
    Parser& p,
    const std::string& expression,
    const Bound& lower,
    const Bound& upper
) {
    if (
        lower.type == FINITE_BOUND &&
        upper.type == FINITE_BOUND
    )
        return finiteIntegrate(
            p,
            expression,
            lower.value,
            upper.value
        );

        if (
            lower.type == NEGATIVE_INFINITY &&
            upper.type == POSITIVE_INFINITY
        ) {
            return
            checkedNegativeInfinity(
                p,
                expression,
                0
            ) +
            checkedPositiveInfinity(
                p,
                expression,
                0
            );
        }

        if (
            lower.type == NEGATIVE_INFINITY &&
            upper.type == FINITE_BOUND
        )
            return checkedNegativeInfinity(
                p,
                expression,
                upper.value
            );

            if (
                lower.type == FINITE_BOUND &&
                upper.type == POSITIVE_INFINITY
            )
                return checkedPositiveInfinity(
                    p,
                    expression,
                    lower.value
                );

                throw std::runtime_error(
                    "invalid integration bounds"
                );

}

double solve(
    Parser& p,
    const std::string& expression,
    double guess
) {
    double x = guess;

    for (int i = 0; i < 100; ++i) {
        double y =
        realEval(
            p,
            expression,
            x
        );

        if (std::abs(y) < 1e-10)
            return x;

        double d =
        derivative(
            p,
            expression,
            x
        );

        if (std::abs(d) < 1e-12)
            throw std::runtime_error(
                "derivative too small"
            );

        double nx = x - y / d;

        if (!std::isfinite(nx))
            throw std::runtime_error(
                "solver diverged"
            );

        if (std::abs(nx - x) < 1e-10)
            return nx;

        x = nx;
    }

    throw std::runtime_error(
        "solver did not converge"
    );

}

bool polynomialIntegral(
    const std::string& expression,
    std::string& result
) {
    std::string s = expression;

    s.erase(
        std::remove_if(
            s.begin(),
                       s.end(),
                       [](unsigned char c) {
                           return std::isspace(c);
                       }
        ),
        s.end()
    );

    if (s == "x") {
        result = "x^2/2 + c";
        return true;
    }

    if (s == "x^2") {
        result = "x^3/3 + c";
        return true;
    }

    if (s == "x^3") {
        result = "x^4/4 + c";
        return true;
    }

    if (s == "1/x") {
        result = "ln(abs(x)) + c";
        return true;
    }

    return false;

}

std::string derivativeRule(
    const std::string& expression
) {
    std::string s =
    lower(trim(expression));

    if (s == "x")
        return "the derivative of x is 1";

    if (s == "x^2")
        return "the power rule gives 2x";

    if (s == "x^3")
        return "the power rule gives 3x^2";

    if (s == "sin(x)")
        return "the derivative of sin x is cos x";

    if (s == "cos(x)")
        return "the derivative of cos x is negative sin x";

    if (s == "tan(x)")
        return "the derivative of tan x is sec squared x";

    if (s == "ln(x)")
        return "the derivative of ln x is 1 over x";

    if (
        s == "exp(x)" ||
        s == "e^x"
    )
        return "the derivative of e to the x is e to the x";

        return "the derivative was calculated numerically";

}

double limit(
    Parser& p,
    const std::string& expression,
    double a
) {
    double h = 1e-5;

    double left =
    realEval(
        p,
        expression,
        a - h
    );

    double right =
    realEval(
        p,
        expression,
        a + h
    );

    if (std::abs(left - right) > 1e-3)
        throw std::runtime_error(
            "limit does not appear to exist"
        );

    return (left + right) / 2.0;

}

void graph(
    Parser& p,
    const std::string& expression
) {
    double xmin = -5;
    double xmax = 5;
    double ymin = -5;
    double ymax = 5;

    std::string input;

    std::cout << "\nx min > ";
    std::getline(std::cin, input);
    if (!trim(input).empty())
        xmin = number(input);

    std::cout << "x max > ";
    std::getline(std::cin, input);
    if (!trim(input).empty())
        xmax = number(input);

    std::cout << "y min > ";
    std::getline(std::cin, input);
    if (!trim(input).empty())
        ymin = number(input);

    std::cout << "y max > ";
    std::getline(std::cin, input);
    if (!trim(input).empty())
        ymax = number(input);

    if (xmin >= xmax)
        throw std::runtime_error(
            "x min must be less than x max"
        );

    if (ymin >= ymax)
        throw std::runtime_error(
            "y min must be less than y max"
        );

    const int width = 81;
    const int height = 31;
    const int samples = 5000;

    std::vector<std::string> screen(
        height,
        std::string(width, ' ')
    );

    auto toCol =
    [&](double x) -> int {
        return (int)std::round(
            (x - xmin) /
            (xmax - xmin) *
            (width - 1)
        );
    };

    auto toRow =
    [&](double y) -> int {
        return (int)std::round(
            (ymax - y) /
            (ymax - ymin) *
            (height - 1)
        );
    };

    auto niceStep =
    [](double range) -> double {
        if (range <= 0)
            return 1;

        double raw = range / 8.0;

        double power =
        std::pow(
            10.0,
            std::floor(
                std::log10(raw)
            )
        );

        double n = raw / power;

        if (n <= 1)
            n = 1;
        else if (n <= 2)
            n = 2;
        else if (n <= 5)
            n = 5;
        else
            n = 10;

        return n * power;
    };

    auto drawLine =
    [&](int x1,
        int y1,
        int x2,
        int y2) {
        int dx = std::abs(x2 - x1);
        int dy = std::abs(y2 - y1);

        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;

        int err = dx - dy;

        while (true) {
            if (
                x1 >= 0 &&
                x1 < width &&
                y1 >= 0 &&
                y1 < height
            ) {
                if (screen[y1][x1] == ' ')
                    screen[y1][x1] = '*';
            }

            if (x1 == x2 && y1 == y2)
                break;

            int e2 = 2 * err;

            if (e2 > -dy) {
                err -= dy;
                x1 += sx;
            }

            if (e2 < dx) {
                err += dx;
                y1 += sy;
            }
        }
        };

        std::vector<double> xs(samples);
        std::vector<double> ys(samples);
        std::vector<bool> valid(samples, false);

        for (int i = 0; i < samples; ++i) {
            double t =
            (double)i /
            (double)(samples - 1);

            double xx =
            xmin +
            (xmax - xmin) * t;

            xs[i] = xx;

            try {
                double y =
                realEval(
                    p,
                    expression,
                    xx
                );

                if (!std::isfinite(y))
                    continue;

                ys[i] = y;
                valid[i] = true;
            }
            catch (...) {
                valid[i] = false;
            }
        }

        double visibleRange = ymax - ymin;

        for (int i = 1; i < samples; ++i) {
            if (!valid[i - 1] || !valid[i])
                continue;

            double y1 = ys[i - 1];
            double y2 = ys[i];

            if (std::abs(y2 - y1) > visibleRange * 0.8)
                continue;

            int x1 = toCol(xs[i - 1]);
            int y1p = toRow(y1);
            int x2 = toCol(xs[i]);
            int y2p = toRow(y2);

            if (std::abs(x2 - x1) > 2)
                continue;

            drawLine(
                x1,
                y1p,
                x2,
                y2p
            );
        }

        bool hasYAxis = xmin <= 0 && xmax >= 0;
        bool hasXAxis = ymin <= 0 && ymax >= 0;

        int xAxisRow =
        hasXAxis ? toRow(0) : -1;

        int yAxisCol =
        hasYAxis ? toCol(0) : -1;

        if (hasXAxis) {
            for (int c = 0; c < width; ++c) {
                if (
                    xAxisRow >= 0 &&
                    xAxisRow < height &&
                    screen[xAxisRow][c] == ' '
                )
                    screen[xAxisRow][c] = '-';
            }
        }

        if (hasYAxis) {
            for (int r = 0; r < height; ++r) {
                if (
                    yAxisCol >= 0 &&
                    yAxisCol < width &&
                    screen[r][yAxisCol] == ' '
                )
                    screen[r][yAxisCol] = '|';
            }
        }

        if (
            hasXAxis &&
            hasYAxis &&
            xAxisRow >= 0 &&
            xAxisRow < height &&
            yAxisCol >= 0 &&
            yAxisCol < width
        )
            screen[xAxisRow][yAxisCol] = '+';

            double xStep = niceStep(xmax - xmin);
            double yStep = niceStep(ymax - ymin);

            if (hasXAxis) {
                double first =
                std::ceil(xmin / xStep) * xStep;

                for (
                    double xx = first;
                xx <= xmax + xStep * 0.001;
                xx += xStep
                ) {
                    if (std::abs(xx) < xStep * 1e-8)
                        continue;

                    int col = toCol(xx);

                    if (
                        col >= 0 &&
                        col < width &&
                        screen[xAxisRow][col] == '-'
                    )
                        screen[xAxisRow][col] = '+';
                }
            }

            if (hasYAxis) {
                double first =
                std::ceil(ymin / yStep) * yStep;

                for (
                    double yy = first;
                yy <= ymax + yStep * 0.001;
                yy += yStep
                ) {
                    if (std::abs(yy) < yStep * 1e-8)
                        continue;

                    int row = toRow(yy);

                    if (
                        row >= 0 &&
                        row < height &&
                        screen[row][yAxisCol] == '|'
                    )
                        screen[row][yAxisCol] = '+';
                }
            }

            std::cout << "\n";

            for (int r = 0; r < height; ++r)
                std::cout << screen[r] << "\n";

    std::cout << "\n";

    if (hasXAxis) {
        std::string labels(width, ' ');

        double first =
        std::ceil(xmin / xStep) * xStep;

        for (
            double xx = first;
        xx <= xmax + xStep * 0.001;
        xx += xStep
        ) {
            int col = toCol(xx);

            std::string label = fmt(xx, 4);

            int start =
            col -
            (int)label.size() / 2;

            for (size_t j = 0; j < label.size(); ++j) {
                int p =
                start +
                (int)j;

                if (p >= 0 && p < width)
                    labels[p] = label[j];
            }
        }

        std::cout << labels << "\n";
    }

    std::cout
    << "x: ["
    << fmt(xmin)
    << ", "
    << fmt(xmax)
    << "]    y: ["
    << fmt(ymin)
    << ", "
    << fmt(ymax)
    << "]\n\n";

}

void table(
    Parser& p,
    const std::string& expression
) {
    std::string input;

    std::cout << "start x > ";
    std::getline(std::cin, input);
    double start = number(input);

    std::cout << "end x > ";
    std::getline(std::cin, input);
    double end = number(input);

    std::cout << "step > ";
    std::getline(std::cin, input);
    double step = number(input);

    if (step <= 0)
        throw std::runtime_error(
            "step must be positive"
        );

    std::cout << "\nx       y\n";

    for (
        double xx = start;
    xx <= end + step * 0.001;
    xx += step
    ) {
        double y =
        realEval(
            p,
            expression,
            xx
        );

        std::cout
        << fmt(xx)
        << "       "
        << fmt(y)
        << "\n";
    }

    std::cout << "\n";

}

void statistics() {
    std::string input;

    std::cout
    << "enter values separated by spaces > ";

    std::getline(
        std::cin,
        input
    );

    std::stringstream ss(input);
    std::vector<double> v;

    double x;

    while (ss >> x)
        v.push_back(x);

    if (v.empty())
        throw std::runtime_error(
            "no values entered"
        );

    double sum = 0;

    for (size_t i = 0; i < v.size(); ++i)
        sum += v[i];

    double mean =
    sum / v.size();

    double variance = 0;

    for (size_t i = 0; i < v.size(); ++i)
        variance +=
        (v[i] - mean) *
        (v[i] - mean);

    variance /= v.size();

    std::vector<double> sorted = v;

    std::sort(
        sorted.begin(),
              sorted.end()
    );

    double median;

    if (sorted.size() % 2)
        median =
        sorted[
            sorted.size() / 2
        ];
    else
        median =
        (
            sorted[
                sorted.size() / 2 - 1
            ] +
            sorted[
                sorted.size() / 2
            ]
        ) / 2.0;

    std::cout
    << "\ncount = "
    << v.size()
    << "\n";

    std::cout
    << "mean = "
    << fmt(mean)
    << "\n";

    std::cout
    << "median = "
    << fmt(median)
    << "\n";

    std::cout
    << "minimum = "
    << fmt(sorted.front())
    << "\n";

    std::cout
    << "maximum = "
    << fmt(sorted.back())
    << "\n";

    std::cout
    << "variance = "
    << fmt(variance)
    << "\n";

    std::cout
    << "standard deviation = "
    << fmt(std::sqrt(variance))
    << "\n\n";

}

void quadratic() {
    std::string input;

    std::cout << "a > ";
    std::getline(std::cin, input);
    double a = number(input);

    std::cout << "b > ";
    std::getline(std::cin, input);
    double b = number(input);

    std::cout << "c > ";
    std::getline(std::cin, input);
    double c = number(input);

    if (std::abs(a) < 1e-15)
        throw std::runtime_error(
            "a cannot be zero"
        );

    Complex d(
        b * b - 4 * a * c,
        0
    );

    Complex r1 =
    (-b + std::sqrt(d)) /
    (2.0 * a);

    Complex r2 =
    (-b - std::sqrt(d)) /
    (2.0 * a);

    std::cout
    << "\nroot one = "
    << complexText(r1)
    << "\n";

    std::cout
    << "root two = "
    << complexText(r2)
    << "\n\n";

}

void complexMenu(Parser& parser) {
    std::string expression;

    std::cout
    << "\ncomplex numbers\n\n"
    << "examples\n"
    << "  3 + 4i\n"
    << "  (2 + 3i)(4 - i)\n"
    << "  sqrt(-1)\n"
    << "  conj(3 + 4i)\n"
    << "  real(3 + 4i)\n"
    << "  imag(3 + 4i)\n"
    << "  arg(3 + 4i)\n"
    << "  norm(3 + 4i)\n\n";

    std::cout
    << "enter complex expression > ";

    std::getline(
        std::cin,
        expression
    );

    Complex z =
    parser.evaluate(expression);

    std::cout
    << "\nvalue = "
    << complexText(z)
    << "\n";

    std::cout
    << "real = "
    << fmt(z.real())
    << "\n";

    std::cout
    << "imaginary = "
    << fmt(z.imag())
    << "\n";

    std::cout
    << "magnitude = "
    << fmt(std::abs(z))
    << "\n";

    std::cout
    << "argument = "
    << fmt(std::arg(z))
    << "\n";

    std::cout
    << "norm = "
    << fmt(std::norm(z))
    << "\n\n";

}

void help() {
    std::cout
    << "\nhelp:\n\n"
    << "enter an expression to calculate it\n"
    << "functions of x open the calculation menu\n"
    << "graph creates an ascii graph\n"
    << "table creates a value table\n"
    << "statistics calculates statistics\n"
    << "quadratic calculates quadratic roots\n"
    << "complex opens complex number tools\n"
    << "clear clears the terminal\n"
    << "help shows this menu\n"
    << "exit quits the calculator\n\n"

    << "operators:\n"
    << "  +   addition\n"
    << "  -   subtraction\n"
    << "  *   multiplication\n"
    << "  /   division\n"
    << "  ^   exponentiation\n"
    << "  !   factorial / gamma(x + 1)\n\n"

    << "implicit multiplication:\n"
    << "  2x\n"
    << "  2(x + 1)\n"
    << "  (x + 1)(x - 1)\n"
    << "  2sin(x)\n"
    << "  2pi\n\n"

    << "common functions:\n"
    << "  sin cos tan\n"
    << "  asin acos atan atan2\n"
    << "  sinh cosh tanh\n"
    << "  asinh acosh atanh\n"
    << "  sqrt cbrt\n"
    << "  exp exp2 expm1\n"
    << "  ln log log10 log2 log1p\n"
    << "  pow hypot\n"
    << "  abs fabs\n"
    << "  gamma tgamma lgamma\n"
    << "  erf erfc\n"
    << "  ceil floor trunc round\n"
    << "  nearbyint rint\n"
    << "  fmod remainder\n"
    << "  fmax fmin fdim\n"
    << "  copysign fma\n"
    << "  ldexp scalbn ilogb logb\n"
    << "  nextafter\n"
    << "  isfinite isinf isnan isnormal\n"
    << "  signbit fpclassify\n\n"

    << "complex functions:\n"
    << "  real imag conj arg norm\n\n"

    << "infinite integrals:\n"
    << "  inf\n"
    << "  -inf\n"
    << "  infinity\n"
    << "  -infinity\n"
    << "  ∞\n"
    << "  -∞\n\n";

}

int main() {
    Parser parser;
    std::string input;

    std::cout
    << "calc V1.8 | expanded math engine\n"
    << "factorials, gamma, improved precedence\n"
    << "improved implicit multiplication\n"
    << "type help for commands\n\n";

    while (true) {
        std::cout
        << "enter expression > ";

        if (!std::getline(
            std::cin,
            input
        ))
            break;

            input = trim(input);

            if (input.empty())
                continue;

        std::string command =
        lower(input);

        if (
            command == "exit" ||
            command == "quit"
        )
            break;

            if (command == "help") {
                help();
                continue;
            }

            if (command == "clear") {
                std::cout
                << "\033[2J\033[H";
                continue;
            }

            if (command == "statistics") {
                try {
                    statistics();
                }
                catch (const std::exception& e) {
                    std::cout
                    << "\nerror "
                    << e.what()
                    << "\n\n";
                }

                continue;
            }

            if (command == "quadratic") {
                try {
                    quadratic();
                }
                catch (const std::exception& e) {
                    std::cout
                    << "\nerror "
                    << e.what()
                    << "\n\n";
                }

                continue;
            }

            if (command == "complex") {
                try {
                    complexMenu(parser);
                }
                catch (const std::exception& e) {
                    std::cout
                    << "\nerror "
                    << e.what()
                    << "\n\n";
                }

                continue;
            }

            try {
                bool hasX =
                parser.hasX(input);

                if (!hasX) {
                    Complex result =
                    parser.evaluate(input);

                    std::cout
                    << "\nresult = "
                    << complexText(result)
                    << "\n\n";

                    continue;
                }

                std::cout
                << "\nwhat would you like to do?\n\n"
                << "1 evaluate\n"
                << "2 solve for x\n"
                << "3 derive\n"
                << "4 integrate\n"
                << "5 graph\n"
                << "6 table\n"
                << "7 limit\n"
                << "8 cancel\n\n";

                std::cout
                << "choice > ";

                std::string choice;

                std::getline(
                    std::cin,
                    choice
                );

                choice = trim(choice);

                if (choice == "1") {
                    std::cout
                    << "x > ";

                    std::string xinput;

                    std::getline(
                        std::cin,
                        xinput
                    );

                    double xv =
                    number(xinput);

                    Complex result =
                    parser.evaluate(
                        input,
                        xv
                    );

                    std::cout
                    << "\nvalue = "
                    << complexText(result)
                    << "\n\n";
                }

                else if (choice == "2") {
                    std::cout
                    << "starting guess > ";

                    std::string guessInput;

                    std::getline(
                        std::cin,
                        guessInput
                    );

                    double guess =
                    number(guessInput);

                    double root =
                    solve(
                        parser,
                        input,
                        guess
                    );

                    std::cout
                    << "\nsolution x = "
                    << fmt(root)
                    << "\n";

                    std::cout
                    << "verification = "
                    << fmt(
                        realEval(
                            parser,
                            input,
                            root
                        )
                    )
                    << "\n\n";
                }

                else if (choice == "3") {
                    std::cout
                    << "\nderivative\n\n"
                    << derivativeRule(input)
                    << "\n\n";

                    std::cout
                    << "x > ";

                    std::string xinput;

                    std::getline(
                        std::cin,
                        xinput
                    );

                    double xv =
                    number(xinput);

                    std::cout
                    << "\nfirst derivative = "
                    << fmt(
                        derivative(
                            parser,
                            input,
                            xv
                        )
                    )
                    << "\n";

                    std::cout
                    << "second derivative = "
                    << fmt(
                        secondDerivative(
                            parser,
                            input,
                            xv
                        )
                    )
                    << "\n\n";
                }

                else if (choice == "4") {
                    std::cout
                    << "\nintegration\n\n"
                    << "1 indefinite integral\n"
                    << "2 definite integral\n"
                    << "3 cancel\n\n";

                    std::cout
                    << "choice > ";

                    std::string ic;

                    std::getline(
                        std::cin,
                        ic
                    );

                    if (ic == "1") {
                        std::string result;

                        if (
                            polynomialIntegral(
                                input,
                                result
                            )
                        ) {
                            std::cout
                            << "\nintegral = "
                            << result
                            << "\n\n";
                        }
                        else {
                            std::cout
                            << "\nno symbolic formula is available\n"
                            << "the numerical engine can still calculate "
                            << "definite and improper integrals\n\n";
                        }
                    }

                    else if (
                        ic == "2" ||
                        ic == "67"
                    ) {
                        std::cout
                        << "lower bound > ";

                        std::string ainput;

                        std::getline(
                            std::cin,
                            ainput
                        );

                        std::cout
                        << "upper bound > ";

                        std::string binput;

                        std::getline(
                            std::cin,
                            binput
                        );

                        Bound lower =
                        parseBound(ainput);

                        Bound upper =
                        parseBound(binput);

                        double result =
                        integrateImproper(
                            parser,
                            input,
                            lower,
                            upper
                        );

                        std::cout
                        << "\nresult = "
                        << fmt(result, 10)
                        << "\n\n";
                    }
                }

                else if (choice == "5") {
                    graph(
                        parser,
                        input
                    );
                }

                else if (choice == "6") {
                    table(
                        parser,
                        input
                    );
                }

                else if (choice == "7") {
                    std::cout
                    << "limit x approaches > ";

                    std::string ainput;

                    std::getline(
                        std::cin,
                        ainput
                    );

                    double a =
                    number(ainput);

                    double result =
                    limit(
                        parser,
                        input,
                        a
                    );

                    std::cout
                    << "\nlimit = "
                    << fmt(result)
                    << "\n\n";
                }

                else if (choice == "8") {
                    std::cout
                    << "\ncancelled\n\n";
                }

                else {
                    std::cout
                    << "\ninvalid choice\n\n";
                }
            }
            catch (const std::exception& e) {
                std::cout
                << "\nerror "
                << e.what()
                << "\n\n";
            }
    }

    std::cout
    << "goodbye!\n";

    return 0;

}
