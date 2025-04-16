#pragma once

#include <glm/glm.hpp>

class KalmanFilter {
public:
    KalmanFilter(float dt, const glm::vec2 &initialState, const glm::mat2 &initialP, const glm::mat2 &A,
                 const glm::mat2 &Q, const glm::vec2 &H, float R)
        : dt(dt), x(initialState), P(initialP), A(A), Q(Q), H(H), R(R) {}

    void predict() {
        x = A * x;
        P = A * P * transpose(A) + Q;
    }

    void update(float z) {
        float S = dot(H, P * H) + R;
        glm::vec2 K = (P * H) / S;
        float y = z - dot(H, x);
        x = x + K * y;
        glm::mat2 I(1.0f);
        P = (I - outerProduct(K, H)) * P;
    }

    void setA(const glm::mat2& newA) {
        A = newA;
    }

    [[nodiscard]] glm::vec2 getState() const { return x; }

private:
    float dt;
    glm::vec2 x;
    glm::mat2 P;
    glm::mat2 A;
    glm::mat2 Q;
    glm::vec2 H;
    float R;
};