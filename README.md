# bayes_fss

Feature subset selection for Naive Bayes classification.


## Purpose

This is a small tool for automatically improving the performance of a Naive
Bayes classifier. Given a labelled dataset consisting in a number of categorized
features, it creates several classifiers that each use a distinct feature subset
and evaluates their performance through cross-validation, looking for the best
performing one. When an optimal feature subset is found, processing stops, and a
summary of the achieved performance is displayed.


## Example

Look at the [sample
dataset](https://github.com/michaelnmmeyer/bayes_fss/blob/master/test/data/cars.tsv)
in the `test/data` directory. It uses six features to predict the quality of a
car: number of doors, price, etc. We can check the performance of the Naive
Bayes classifier that uses all these features as follows: 

    $ bayes_fss --search=none test/data/cars.tsv
    {
       "subset": ["buying","maint","doors","persons","lug_boot","safety"],
       "accuracy": 92.956522,
       "precision": 77.187566,
       "recall": 58.949352,
       "F1": 64.245426,
       "subsets_evaluated": 2,
       "interrupted": false
    }

Can we do better? Lets check:

    $ bayes_fss --search=backward-join test/data/cars.tsv
    {
       "subset": [["buying","maint"],["doors","lug_boot","safety"],"persons"],
       "accuracy": 97.159420,
       "precision": 94.168320,
       "recall": 86.875141,
       "F1": 90.011069,
       "subsets_evaluated": 54,
       "interrupted": false
    }

The above means that, to obtain a better performance, the `buying` and `maint`
features should be merged into a single feature, as well as the features
`doors`, `lug_boot` and `safety`, so that merely three features remain.

For full details, see the PDF manual in the `doc` directory, or type `man
bayes_fss` after installation.

## Building

You need a C11 compiler, which typically means GCC or Clang on Unix. You can
then invoke the usual:

    $ make && sudo make install

There is no other dependency than the C standard library.

## References

* [Pazzani (1996), Searching for dependencies in Bayesian
  classifiers](http://www.cs.rutgers.edu/~pazzani/Publications/Pazzani-aistats96.pdf).
* [Blanco et al. (2005), Feature selection in Bayesian classifiers for the
  prognosis of survival of cirrhotic patients treated with
  TIPS](http://www.sciencedirect.com/science/article/pii/S153204640500047X).
* [Zheng & Webb (2008), Semi-naive Bayesian
  Classification](http://www.csse.monash.edu/~webb/Files/ZhengWebb08a.pdf).
