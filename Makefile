all: web_proxy

web_proxy : main.cpp
	g++ -o web_proxy main.cpp -lpthread

clean:
	rm web_proxy
