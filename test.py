import csv
with open("data.csv", "a", newline="") as fdata:
	fhandler = csv.writer(fdata)
	fhandler.writerow(["PassengerId","Survived","test1"])
	fdata.close()
print('Done.')
