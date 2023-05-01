int main() {
	float k = 101.0010101001;
	float a = 10, b = 100;
	while (1) {
		k = a * (b / k + a / k ) + k;
		a = a + k / a + b / k;
		b = a + k / a + a / k;
	}
}