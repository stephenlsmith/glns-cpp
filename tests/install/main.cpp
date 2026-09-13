#include <glns/glns.hpp>

int main() {
    glns::Matrix matrix(2, 0);
    matrix.set(0, 1, 1);
    return matrix(0, 1) == 1 ? 0 : 1;
}
