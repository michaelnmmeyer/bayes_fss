# bayes_fss

Feature subset selection for Naive Bayes classification.


## Purpose

This is a small tool for selecting features subsets so as to improve the
performance of a Naive Bayes classifier. Given a dataset, it evaluates several
features subsets in turn and outputs an optimal one. It can also learn
dependencies between features, which helps to improve performance.

In short:

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

I wrote this program because I was tired of spending time manually adjusting and
testing features for my NLP stuff, and wanted a convenient program to do that
work for me. This is the intended use case. For more complex scenarios (and more
bloat), you can use Weka.

## Building

You need a C11 compiler, which typically means GCC or Clang on Unix. You can
then invoke the usual:

    $ make && sudo make install

There is no other dependency than the C standard library.

## Usage

For full details, see the PDF manual in the `doc` directory, or type `man
bayes_fss` after installation.

As concerns adjusting your code to take into account the suggestions made by
this program, nothing special needs to done except writing a function to merge
together dependent features before feeding them to your classifier. For example,
in Python:

    FEATURES = [("buying","maint"),("doors","lug_boot","safety"),("persons",)]
    
    def join_features(raw_features):
        # raw_features is a dict {"buying":"foo", "maint":"bar"...}
	    new_features = {}
	    for feature in FEATURES:
           new_features[feature] = tuple(raw_features[sub_feature] for sub_feature in feature)
        return new_features
 
You should also take care that the smoothing parameter of your classifier is the
same as the one used by this program.

## References

* Pazzani (1996), Searching for dependencies in Bayesian classifiers.
* Blanco et al. (2005), Feature selection in Bayesian classifiers for the
prognosis of survival of cirrhotic patients treated with TIPS.
* Zheng & Webb (2008), Semi-naive Bayesian Classification.
