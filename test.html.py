import sys
import json

# Parameters passed by WinHost as JSON in argument
params = json.loads(sys.argv[1])

print("<h1>Hello, World!</h1>")
print("<p>This is a Python program being hosted from WinHost!</p>")

# If n1 and n2 in params
if("n1" in params and "n2" in params):
    print("The sum of the numbers is: ", int(params["n1"]) + int(params["n2"]))

