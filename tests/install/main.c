#include <glns/glns.h>

int main(void) {
    glns_params params;
    glns_params_init(&params);
    return params.trials == -1 ? 0 : 1;
}
